/*
 * This file is part of EasyRPG Player.
 *
 * EasyRPG Player is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * EasyRPG Player is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with EasyRPG Player. If not, see <http://www.gnu.org/licenses/>.
 */

#include <array>
#include <algorithm>
#include "audio_decoder_midi.h"
#include "midisequencer.h"
#include "output.h"

constexpr int AudioDecoderMidi::midi_default_tempo;

// 1 ms of MIDI message resolution for a 44100 Hz samplerate
constexpr int samples_per_play = 512;
constexpr int bytes_per_sample = sizeof(int16_t) * 2;

static const uint8_t midi_event_control_change = 0b1011;
static const uint8_t midi_control_volume = 7;
static const uint8_t midi_control_all_sound_off = 120;
static const uint8_t midi_control_all_note_off = 123;
static const uint8_t midi_control_reset_all_controller = 121;

static uint32_t midimsg_make(uint8_t event_type, uint8_t channel, uint8_t value1, uint8_t value2) {
	uint32_t msg = 0;
	msg |= (((event_type << 4) & 0xF0) | (channel & 0x0F)) & 0x0000FF;
	msg |= (value1 << 8) & 0x00FF00;
	msg |= (value2 << 16) & 0xFF0000;
	return msg;
}

static uint32_t midimsg_all_note_off(uint8_t channel) {
	return midimsg_make(midi_event_control_change, channel, midi_control_all_note_off, 0);
}

static uint32_t midimsg_all_sound_off(uint8_t channel) {
	return midimsg_make(midi_event_control_change, channel, midi_control_all_sound_off, 0);
}

static uint32_t midimsg_volume(uint8_t channel, uint8_t volume) {
	return midimsg_make(midi_event_control_change, channel, midi_control_volume, volume);
}

static uint32_t midimsg_reset_all_controller(uint8_t channel) {
	return midimsg_make(midi_event_control_change, channel, midi_control_reset_all_controller, 0);
}

static inline uint8_t midimsg_get_event_type(uint32_t msg) {
	return (msg & 0x0000F0) >> 4;
}

static inline uint8_t midimsg_get_channel(uint32_t msg) {
	return (msg & 0x00000F);
}

static inline uint8_t midimsg_get_value1(uint32_t msg) {
	return (msg & 0x00FF00) >> 8;
}

static inline uint8_t midimsg_get_value2(uint32_t msg) {
	return (msg & 0xFF0000) >> 16;
}

AudioDecoderMidi::AudioDecoderMidi(std::unique_ptr<MidiDecoder> mididec)
	: mididec(std::move(mididec)) {
	seq = std::make_unique<midisequencer::sequencer>();
	channel_volumes.fill(127);
}

AudioDecoderMidi::~AudioDecoderMidi() {
	reset();
}

static int read_func(void* instance) {
	AudioDecoderMidi* midi = reinterpret_cast<AudioDecoderMidi*>(instance);

	if (midi->file_buffer_pos >= midi->file_buffer.size()) {
		return EOF;
	}

	return midi->file_buffer[midi->file_buffer_pos++];
}

bool AudioDecoderMidi::Open(Filesystem_Stream::InputStream stream) {
	Reset();
	seq->clear();

	file_buffer_pos = 0;
	file_buffer = Utils::ReadStream(stream);

	if (!seq->load(this, read_func)) {
		error_message = "Midi: Error reading file";
		return false;
	}
	seq->rewind();
	mtime = seq->get_start_skipping_silence();

	/* FIXME if (!mididec->Open(file_buffer)) {
		error_message = "Internal Midi: Error reading file";
		return false;
	}*/

	tempo.emplace_back(this, midi_default_tempo);

	return true;
}

void AudioDecoderMidi::Pause() {
	paused = true;
	for (int i = 0; i < 16; i++) {
		uint32_t msg = midimsg_volume(i, 0);
		mididec->SendMidiMessage(msg);
	}
}

void AudioDecoderMidi::Resume() {
	paused = false;
	for (int i = 0; i < 16; i++) {
		uint32_t msg = midimsg_volume(i, static_cast<uint8_t>(channel_volumes[i] * volume));
		mididec->SendMidiMessage(msg);
	}
}

int AudioDecoderMidi::GetVolume()const {
	if (fade_steps > 0) {
		return static_cast<int>(fade_end * 100);
	}
	return static_cast<int>(volume * 100);
}

void AudioDecoderMidi::SetVolume(int new_volume) {
	// cancel any pending fades
	fade_steps = 0;

	volume = new_volume / 100.0f;
	for (int i = 0; i < 16; i++) {
		uint32_t msg = midimsg_volume(i, static_cast<uint8_t>(channel_volumes[i] * volume));
		mididec->SendMidiMessage(msg);
	}
}

void AudioDecoderMidi::SetFade(int begin, int end, int duration) {
	fade_steps = 0;
	last_fade_mtime = 0.0f;

	if (duration <= 0.0) {
		SetVolume(end);
		return;
	}

	if (begin == end) {
		SetVolume(end);
		return;
	}

	volume = begin / 100.0f;
	fade_end = end / 100.0f;
	fade_steps = duration / 100;
	delta_step = (fade_end - volume) / fade_steps;
}

bool AudioDecoderMidi::Seek(std::streamoff offset, std::ios_base::seekdir origin) {
	assert(!tempo.empty());

	if (offset == 0 && origin == std::ios_base::beg) {
		mtime = seq->rewind_to_loop();

		// When the loop points to the end of the track keep it alive to match
		// RPG_RT behaviour.
		loops_to_end = mtime >= seq->get_total_time();

		if (mtime > 0.0f) {
			// Throw away all tempo data after the loop point
			auto rit = std::find_if(tempo.rbegin(), tempo.rend(), [&](auto& t) { return t.mtime <= mtime; });
			auto it = rit.base();
			if (it != tempo.end()) {
				tempo.erase(it, tempo.end());
			}

			// Bit of a hack, prevent stuck notes
			// TODO: verify with a MIDI event stream inspector whether RPG_RT does this?
			// FIXME synth->all_note_off();
		}
		else {
			tempo.clear();
			tempo.emplace_back(this, midi_default_tempo);
		}

		/* FIXME if (mididec->GetName() == "WildMidi") {
			mididec->Seek(tempo.back().GetSamples(mtime), origin);
		}
		else {
			mididec->Seek(GetTicks(), origin);
		}*/

		return true;
	}

	return false;
}

bool AudioDecoderMidi::IsFinished() const {
	if (loops_to_end) {
		return false;
	}

	return seq->is_at_end();
}

void AudioDecoderMidi::Update(int delta) {
	if (paused) {
		return;
	}
	if (fade_steps >= 0 && mtime - last_fade_mtime > 0.1f) {
		volume = Utils::Clamp<float>(volume + delta_step, 0.0f, 1.0f);
		for (int i = 0; i < 16; i++) {
			uint32_t msg = midimsg_volume(i, static_cast<uint8_t>(channel_volumes[i] * volume));
			mididec->SendMidiMessage(msg);
		}
		last_fade_mtime = mtime;
		fade_steps -= 1;
	}

	seq->play(mtime, this);
	mtime += delta * pitch / 100.0 / 1000.0;

	if (IsFinished() && looping) {
		mtime = seq->rewind_to_loop();
		reset_tempos_after_loop();
	}
}

void AudioDecoderMidi::GetFormat(int& freq, AudioDecoderBase::Format& format, int& channels) const {
	mididec->GetFormat(freq, format, channels);
}

bool AudioDecoderMidi::SetFormat(int freq, AudioDecoderBase::Format format, int channels) {
	frequency = freq;
	return mididec->SetFormat(freq, format, channels);
}

bool AudioDecoderMidi::SetPitch(int pitch) {
	this->pitch = pitch;
	return true;
}

int AudioDecoderMidi::GetTicks() const {
	assert(!tempo.empty());

	return tempo.back().GetTicks(mtime);
}

void AudioDecoderMidi::Reset() {
	// Generate a MIDI reset event so the device doesn't
	// leave notes playing or keeps any state
	reset();
}

int AudioDecoderMidi::FillBuffer(uint8_t* buffer, int length) {
	if (loops_to_end) {
		memset(buffer, '\0', length);
		return length;
	}

	int samples_max = length / bytes_per_sample;
	int written = 0;

	// Advance the MIDI playback in smaller steps to achieve a 1ms message resolution
	// Otherwise the MIDI sounds off because messages are processed too late.
	while (samples_max > 0) {
		// Process MIDI messages
		size_t samples = std::min(samples_per_play, samples_max);
		float delta = (float)samples / (frequency * pitch / 100.0f);
		seq->play(mtime, this);
		mtime += delta;

		// Write audio samples
		int len = samples * bytes_per_sample;
		int res = mididec->FillBuffer(buffer + written, len);
		written += res;

		if (samples < samples_per_play || res < len) {
			// Done
			break;
		}

		samples_max -= samples;
	}

	return written;
}

void AudioDecoderMidi::SendMessageToAllChannels(uint32_t midi_msg) {
	for (int channel = 0; channel < 16; channel++) {
		uint8_t event_type = midimsg_get_event_type(midi_msg);
		midi_msg |= (((event_type << 4) & 0xF0) | (channel & 0x0F)) & 0x0000FF;
		mididec->SendMidiMessage(midi_msg);
	}
}

void AudioDecoderMidi::midi_message(int, uint_least32_t message) {
	uint8_t event_type = midimsg_get_event_type(message);
	uint8_t channel = midimsg_get_channel(message);
	uint8_t value1 = midimsg_get_value1(message);
	uint8_t value2 = midimsg_get_value2(message);

	if (event_type == midi_event_control_change && value1 == midi_control_volume) {
		// Adjust channel volume
		channel_volumes[channel] = value2;
		// Send the modified volume to midiout
		message = midimsg_volume(channel, static_cast<uint8_t>(value2 * volume));
	}
	mididec->SendMidiMessage(message);
}

void AudioDecoderMidi::sysex_message(int, const void* data, std::size_t size) {
	mididec->SendSysExMessage(data, size);
}

void AudioDecoderMidi::meta_event(int event, const void* data, std::size_t size) {
	// Meta events are never sent over MIDI ports.
	assert(!tempo.empty());
	const auto* d = reinterpret_cast<const uint8_t*>(data);
	if (size == 3 && event == 0x51) {
		uint32_t new_tempo = (static_cast<uint32_t>(static_cast<unsigned char>(d[0])) << 16)
			| (static_cast<unsigned char>(d[1]) << 8)
			| static_cast<unsigned char>(d[2]);
		tempo.emplace_back(this, new_tempo, &tempo.back());
	}
}

void AudioDecoderMidi::reset() {
	// MIDI reset event
	SendMessageToAllChannels(midimsg_all_sound_off(0));
	SendMessageToAllChannels(midimsg_reset_all_controller(0));
	mididec->SendMidiReset();
}

void AudioDecoderMidi::reset_tempos_after_loop() {
	if (mtime > 0.0f) {
		// Throw away all tempo data after the loop point
		auto rit = std::find_if(tempo.rbegin(), tempo.rend(), [&](auto& t) { return t.mtime <= mtime; });
		auto it = rit.base();
		if (it != tempo.end()) {
			tempo.erase(it, tempo.end());
		}
	} else {
		tempo.clear();
		tempo.emplace_back(this, midi_default_tempo);
	}
}

// TODO: Solve the copy-paste job between this and GenericMidiDecoder
AudioDecoderMidi::MidiTempoData::MidiTempoData(const AudioDecoderMidi* midi, uint32_t cur_tempo, const MidiTempoData* prev)
	: tempo(cur_tempo) {
	ticks_per_sec = (float)midi->seq->get_division() / tempo * 1000000;
	mtime = midi->mtime;
	if (prev) {
		float delta = mtime - prev->mtime;
		int ticks_since_last = static_cast<int>(ticks_per_sec * delta);
		ticks = prev->ticks + ticks_since_last;
	}
}

int AudioDecoderMidi::MidiTempoData::GetTicks(float mtime_cur) const {
	float delta = mtime_cur - mtime;
	return ticks + static_cast<int>(ticks_per_sec * delta);
}
