# EasyRPG Player

EasyRPG Player is a game interpreter to play RPG Maker 2000, 2003 and EasyRPG
games. It uses the LCF parser library (liblcf) to read RPG Maker game data.

EasyRPG Player is part of the EasyRPG Project. More information is
available at the project website: https://easyrpg.org/


## Documentation

Documentation is available at the documentation wiki: https://wiki.easyrpg.org


## Requirements

### minimal / required

- [liblcf] for RPG Maker data reading.
- SDL2 for screen backend support.
- Pixman for low level pixel manipulation.
- libpng for PNG image support.
- zlib for XYZ image support.
- fmtlib for interal logging.

### extended / recommended

- FreeType2 for external font support (+ HarfBuzz for Unicode text shaping)
- mpg123 for better MP3 audio support
- WildMIDI for better MIDI audio support
- Libvorbis / Tremor for Ogg Vorbis audio support
- opusfile for Opus audio support
- libsndfile for better WAVE audio support
- libxmp for better tracker music support
- SpeexDSP for proper audio resampling

SDL 1.2 is still supported, but deprecated.


## Daily builds

Up to date binaries for assorted platforms are available at our continuous
integration service:

https://ci.easyrpg.org/view/Player/


## Source code

EasyRPG Player development is hosted by GitHub, project files are available
in this git repository:

https://github.com/EasyRPG/Player

Released versions are also available at our Download Archive:

https://easyrpg.org/downloads/player/


## Building

### Dependencies:

If your operating system has a package manager, we recommend installing the
dependencies with it.

In case you want to compile the dependencies yourself, you can find them,
except for [liblcf], in our [buildscripts] repository.


### Autotools Makefile method:

Building requirements:

- pkg-config
- GNU make

Step-by-step instructions:

    tar xf easyrpg-player-0.7.tar.xz # unpack the tarball
    cd easyrpg-player-0.7            # enter in the package directory
    ./configure                      # find libraries, set options
    make                             # compile the executable

Additional building requirements when using the source tree (git):

- autoconf >= 2.69
- automake >= 1.11.4
- libtool

To generate the "configure" script, run before following the above section:

    autoreconf -i

Read more detailed instructions at:

https://wiki.easyrpg.org/development/compiling/player/autotools


### CMake method:

Building requirements:

- pkg-config (only on Linux)
- CMake 3.10 or newer

Step-by-step instructions:

    tar xf easyrpg-player-0.7.tar.xz      # unpack the tarball
    cd easyrpg-player-0.7                 # enter in the package directory
    cmake . -DCMAKE_BUILD_TYPE=Release    # configure project
    cmake --build .                       # compile the executable
    sudo cmake --build . --target install # install system-wide

Read more detailed instructions at:

https://wiki.easyrpg.org/development/compiling/player/cmake

CMake is the only supported way to build Player for Windows. All dependencies
must be installed with [vcpkg].


### Building a libretro core:

Building for libretro is based on the CMake method.

Additional commands required before building:

    git submodule init   # Init submodules
    git submodule update # Clone libretro-common submodule

Invoke CMake with these additional parameters:

    cmake . -DPLAYER_TARGET_PLATFORM=libretro -DBUILD_SHARED_LIBS=ON|OFF

Set shared libs to ON or OFF depending on which type of libraries RetroArch
uses on the platform you are targeting.


### Building an Android APK:

Building requirements:

- Android SDK with NDK r21

Step-by-step instructions:

    tar xf easyrpg-player-0.7.tar.xz     # unpack the tarball
    cd easyrpg-player-0.7/builds/android # enter in the android directory
    ./gradlew -PtoolchainDirs="DIR1;DIR2" assembleRelease # create the APK

Replace ``DIR1`` etc. with the path to the player dependencies. You can use
the scripts in the ``android`` folder of our [buildscripts] to compile them.

To pass additional CMake arguments use ``-PcmakeOptions``:

    -PcmakeOptions="-DSOME_OPTION1=ON -DSOME_OPTION2=OFF"

The unsigned APK is stored in:

    app/build/outputs/apk/release/app-release-unsigned.apk


## Running EasyRPG Player

Run the `easyrpg-player` executable from a RPG Maker 2000 or 2003 game
project folder (same place as `RPG_RT.exe`).


## Bug reporting

Available options:

* File an issue at https://github.com/EasyRPG/Player/issues
* Open a thread at https://community.easyrpg.org/
* Chat with us via IRC: [#easyrpg at irc.libera.chat]


## License

EasyRPG Player is free software available under the GPLv3 license. See the file
[COPYING] for license conditions.


### 3rd party software

EasyRPG Player makes use of the following 3rd party software:

* [FMMidi] YM2608 FM synthesizer emulator - Copyright (c) 2003-2006 yuno
  (Yoshio Uno), provided under the (3-clause) BSD license

* [dr_wav] WAV audio loader and writer - Copyright (c) David Reid, provided
  under public domain or MIT-0

* [PicoJSON] JSON parser/serializer - Copyright (c) 2009-2010 Cybozu Labs, Inc.
  Copyright (c) 2011-2015 Kazuho Oku, provided under the (2-clause) BSD license

* [Dirent] interface for Microsoft Visual Studio -
  Copyright (c) 2006-2012 Toni Ronkko, provided under the MIT license

* [rang] terminal color library - by Abhinav Gauniyal, provided under Unlicense

### 3rd party resources

* [Baekmuk] font family (Korean) - Copyright (c) 1986-2002 Kim Jeong-Hwan,
  provided under the Baekmuk License

* [Shinonome] font familiy (Japanese) - Copyright (c) 1999-2000 Yasuyuki
  Furukawa and contributors, provided under public domain. Glyphs were added
  and modified for use in EasyRPG Player, all changes under public domain.

* [ttyp0] font family - Copyright (c) 2012-2015 Uwe Waldmann, provided under
  ttyp0 license

* [WenQuanYi] font family (CJK) - Copyright (c) 2004-2010 WenQuanYi Project
  Contributors provided under the GPLv2 or later with Font Exception

* [Teenyicons] Tiny minimal 1px icons - Copyright (c) 2020 Anja van Staden,
  provided under the MIT license (only used by the Emscripten web shell)

[buildscripts]: https://github.com/EasyRPG/buildscripts
[liblcf]: https://github.com/EasyRPG/liblcf
[vcpkg]: https://github.com/Microsoft/vcpkg
[#easyrpg at irc.libera.chat]: https://kiwiirc.com/nextclient/#ircs://irc.libera.chat/#easyrpg?nick=rpgguest??
[COPYING]: COPYING
[FMMidi]: http://unhaut.epizy.com/fmmidi
[dr_wav]: https://github.com/mackron/dr_libs
[PicoJSON]: https://github.com/kazuho/picojson
[Dirent]: https://github.com/tronkko/dirent
[rang]: https://github.com/agauniyal/rang
[baekmuk]: https://kldp.net/baekmuk
[Shinonome]: http://openlab.ring.gr.jp/efont/shinonome
[ttyp0]: https://people.mpi-inf.mpg.de/~uwe/misc/uw-ttyp0
[WenQuanYi]: http://wenq.org
[Teenyicons]: https://github.com/teenyicons/teenyicons
