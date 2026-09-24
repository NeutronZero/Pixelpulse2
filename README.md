## Pixelpulse2

[![License](https://img.shields.io/badge/license-MPL-blue.svg)](LICENSE)

Pixelpulse is a powerful user interface for visualizing and manipulating signals while exploring systems attached to affordable analog interface devices, such as Analog Devices' ADALM1000.

Fully cross-platform using the Qt5 graphics toolkit and OpenGL accelerated density-gradiated rendering, it provides a powerful and accessible tool for initial interactive explorations.

Intuitive click-and-drag interfaces make exploring system behaviors across a wide range of signal amplitudes, frequencies, or phases a trivial exercise. Just click once to source a constant voltage or current and see what happens. Choose a function (sawtooth, triangle, sinusoidal, square) - adjust parameters, and make waves.

Zoom in and out with your scroll wheel or multitouch gestures (on supported platforms). Hold "Shift" for Y-axis zooming.

Click and drag the X axis to pan in time.

### Screenshot

![Screenshot of PP2 on Windows 7](https://analogdevicesinc.github.io/Pixelpulse2/pp2screenshot.png "Pixelpulse on Windows 7")

### Requirements

* C++17 compiler, CMake 3.18 or newer, Ninja (recommended)
* Qt 5.15 LTS (Qt 6 is not required for this build)
* libsmu (https://github.com/analogdevicesinc/libsmu), which itself needs libusb and Boost headers
* libusb development files

### Getting Pixelpulse2

#### Easy

* Windows - install the WinUSB driver for the M1K (see below), then run a built `pixelpulse2.exe` with its deployed Qt DLLs next to it.
* Linux - build from source (below).

#### Windows driver

The ADALM1000 must be bound to the WinUSB driver. The easiest way is
[Zadig](https://zadig.akeo.ie/): select the `ADALM1000` device and install
the WinUSB driver for it. Without this, the device cannot be opened
(`Access is denied`) by either Pixelpulse2 or the `smu` command line tool.

Do not run two libsmu-based programs against the same device at the same
time; the second one will fail to open the already-claimed device.

### Building from source

#### Windows (MSYS2 MinGW 64-bit, tested)

In an MSYS2 shell:

```bash
pacman -S --noconfirm mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake \
  mingw-w64-x86_64-ninja mingw-w64-x86_64-qt5-base \
  mingw-w64-x86_64-qt5-declarative mingw-w64-x86_64-qt5-quickcontrols \
  mingw-w64-x86_64-qt5-graphicaleffects mingw-w64-x86_64-qt5-svg \
  mingw-w64-x86_64-qt5-tools mingw-w64-x86_64-libusb mingw-w64-x86_64-boost
```

Build and install libsmu first (its own CMake scripts target an older
toolchain, so pass the minimum policy version and explicit libusb paths):

```bash
git clone https://github.com/analogdevicesinc/libsmu.git
cmake -S libsmu -B libsmu-build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/mingw64 -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DLIBUSB_LIBRARIES=/mingw64/lib/libusb-1.0.dll.a \
  -DLIBUSB_INCLUDE_DIRS=/mingw64/include/libusb-1.0 \
  -DBUILD_PYTHON=OFF -DBUILD_EXAMPLES=OFF -DBUILD_TESTS=OFF
cmake --build libsmu-build
cmake --install libsmu-build
```

Then build Pixelpulse2:

```bash
git clone https://github.com/NeutronZero/Pixelpulse2
cd Pixelpulse2
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DLIBSMU_LIBRARIES=/mingw64/lib/libsmu.dll.a \
  -DLIBSMU_INCLUDE_DIRS=/mingw64/include \
  -DLIBUSB_LIBRARIES=/mingw64/lib/libusb-1.0.dll.a \
  -DLIBUSB_INCLUDE_DIRS=/mingw64/include/libusb-1.0
cmake --build build
```

Deploy the Qt runtime next to the executable (from an MSYS2 shell, with
`qml` being this repository's `qml` directory):

```bash
mkdir -p dist
cp build/pixelpulse2.exe dist/
cp /mingw64/bin/libsmu.dll /mingw64/bin/libusb-1.0.dll dist/
windeployqt-qt5 --dir dist --qmldir qml build/pixelpulse2.exe
```

Copy the `platforms` plugin, the `qml` import tree, and the compiler
runtime DLLs reported by `ldd build/pixelpulse2.exe` into `dist/`, then
run `dist/pixelpulse2.exe`.

#### Ubuntu

```bash
sudo apt-get update
sudo apt-get install -y cmake ninja-build qtbase5-dev qtdeclarative5-dev \
  libqt5svg5-dev libqt5opengl5-dev qml-module-qtquick-dialogs \
  qml-module-qtgraphicaleffects qml-module-qtquick-controls \
  qml-module-qtquick-layouts qml-module-qtquick-window2 \
  qml-module-qtqml-models2 libusb-1.0-0-dev libboost-dev
```

Build and install libsmu (https://github.com/analogdevicesinc/libsmu),
then:

```bash
git clone https://github.com/NeutronZero/Pixelpulse2
cd Pixelpulse2
cmake -S . -B build -G Ninja
cmake --build build
```

* Make sure your M1K is plugged into your computer. The onboard LED should light up when it is connected. You can double-check by typing ```lsusb```. You should see something along the lines of ```ID 064b:784c Analog Devices, Inc. (White Mountain DSP)```
* Run Pixelpulse2 from the build directory:

    ```bash
    ./build/pixelpulse2
    ```
