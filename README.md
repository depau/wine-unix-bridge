# Wine native Linux executable wrapper

This program allows you to run a native Linux executable from a Windows program
running under wine.

## Usage

- Copy `unixbridge.exe` and `unixbridge.dll` to a directory accessible by your
  Windows program running under Wine.
- Perform one of the following to tell the wrapper what Linux executable to run:
    - Rename `unixbridge.exe` to the name of the Linux executable you want to
      run, i.e. `ffmpeg.exe` if you want to run `ffmpeg`.
    - Set the `UNIX_BRIDGE_TARGET` environment variable to the path of the Linux
      executable you want to run, i.e. `UNIX_BRIDGE_TARGET=/usr/bin/ffmpeg`
      (note: this is an absolute path)
- Run `unixbridge.exe` from your Windows program. It will launch the specified
  Linux executable and forward its standard input/output/error to the Windows
  program that launched it.

## Building

### Manually

Make sure Wine and MinGW-w64 are installed on your system. If you intend to
build the 32-bit version you must install a non-WoW64 build of Wine.

On a Debian-based system you can install the necessary dependencies with:

```bash
# as root
dpkg --add-architecture i386
apt update
# For 64-bit build:
apt install mingw-w64 build-essential wine wine64 wine64-tools libwine-dev libc6-dev
# For 32-bit build:
apt install mingw-w64 gcc:i386 g++:i386 mingw-w64 wine:i386 wine32 wine32-tools libwine-dev:i386 libc6-dev:i386
```

Then, you can build the program with:

```bash
make -j$(nproc) x64 # for 64-bit build
make -j$(nproc) x86 # for 32-bit build
```

### With Docker

The Dockerfile in this repository can be used to build the program without
having to manually manage the dependencies, especially making it easier to build
the 32-bit version on a 64-bit system.

```bash
# Build the base images for both architectures
docker build --platform=linux/amd64 -t unixbridge-builder:amd64 .
docker build --platform=linux/386 -t unixbridge-builder:i386 .

# Build the program for both architectures
docker run -u $(id -u):$(id -g) --rm -v "$(pwd):/workdir" -w /workdir unixbridge-builder:amd64 make -j$(nproc) x64
docker run -u $(id -u):$(id -g) --rm -v "$(pwd):/workdir" -w /workdir unixbridge-builder:i386 make -j$(nproc) x86
```

## License

This program is licensed under the MIT License. See the LICENSE file for
details.
