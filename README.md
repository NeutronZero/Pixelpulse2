## Pixelpulse2

[![Windows Status](https://ci.appveyor.com/api/projects/status/32r7s2skrgm9ubva?svg=true)](https://ci.appveyor.com/project/analogdevicesinc/pixelpulse2/branch/master)
[![OSX Status](https://api.travis-ci.org/analogdevicesinc/Pixelpulse2.svg?branch=master&label=OSX)](https://travis-ci.org/analogdevicesinc/Pixelpulse2)
[![License](https://img.shields.io/badge/license-MPL-blue.svg)](https://github.com/analogdevicesinc/Pixelpulse2/blob/master/LICENSE)

Pixelpulse is a powerful user interface for visualizing and manipulating signals while exploring systems attached to affordable analog interface devices, such as Analog Devices' ADALM1000.

Fully cross-platform using the Qt5 graphics toolkit and OpenGL accelerated density-gradiated rendering, it provides a powerful and accessible tool for initial interactive explorations.

Intuitive click-and-drag interfaces make exploring system behaviors across a wide range of signal amplitudes, frequencies, or phases a trivial exercise. Just click once to source a constant voltage or current and see what happens. Choose a function (sawtooth, triangle, sinusoidal, square) - adjust parameters, and make waves.

Zoom in and out  with your scroll wheel or multitouch gestures (on supported platforms). Hold "Shift" to for Y-axis zooming.

Click and drag the X axis to pan in time.

### Screenshot

![Screenshot of PP2 on Windows 7](https://analogdevicesinc.github.io/Pixelpulse2/pp2screenshot.png "Pixelpulse on Windows 7")

### Getting Pixelpulse2

#### Easy

* OSX - Navigate to the [releases](https://github.com/analogdevicesinc/pixelpulse2/releases) and collect the latest `pixelpulse2-<OS-version>.dmg` package, specific for you OS version.
* Windows - For a testing build, download the dependency package and the latest binary build from [appveyor](https://ci.appveyor.com/project/analogdevicesinc/pixelpulse2/build/artifacts). For an official release build, navigate to releases and collect the latest pixelpulse2-setup.exe.
* Linux - Build from source (below) 
#### Advanced

To build from source on any platform, you need to install a C++ compiler toolchain, collect the build dependencies, setup your build environment, and compile the project.

If you have not built packages from source before, this is ill-advised.
*  **Build and install libsmu (https://github.com/analogdevicesinc/libsmu)**. 
Libsmu is a library wich contains abstractions for streaming data to and from USB-connected analog interface devices, currently supporting the Analog Devices' ADALM1000. 
* Install Qt5.15 LTS. Qt 6 is not required for this build.
 * On most Linux Distributions, Qt5 is available in repositories. The complete list of packages required varies, but includes qt's support for declarative (qml) UI programming, qtquick, qtquick-window, qtquick-controls, and qtquick-layouts.

To build / run on a generic POSIX platform

    git clone https://github.com/analogdevicesinc/Pixelpulse2
    cd Pixelpulse2
    cmake -S . -B build -G Ninja
    cmake --build build

On Windows the process is similar. Write the following commands in a cmd console

	git clone https://github.com/analogdevicesinc/Pixelpulse2
	cd Pixelpulse2
	mkdir build
	cd build
    cmake -DLIBSMU_LIBRARIES="path_to_libsmu_library" -DLIBSMU_INCLUDE_DIRS="path_to_libsmu_include_folder" -DLIBUSB_LIBRARIES="path_to_libusb_library" -DLIBUSB_INCLUDE_DIRS="path_to_libusb_include_folder" ..
	cmake --build .

After it is finished building, you have to copy the libsmu shared library into the build folder and Pixelpulse2 should be ready to use with your M1K

To build / run on Ubuntu

 * The build process is tested with Qt 5.15 on current Ubuntu LTS releases.

* Get ready

    ```bash
    sudo apt-get update
    ```

* Build and install libsmu (https://github.com/analogdevicesinc/libsmu)

* Install Qt5 and some Qt modules

    ```bash
    sudo apt-get install -y cmake ninja-build qtbase5-dev qtdeclarative5-dev libqt5svg5-dev libqt5opengl5-dev qml-module-qtquick-dialogs qml-module-qtgraphicaleffects qml-module-qtquick-controls qml-module-qtquick-layouts qml-module-qtquick-window2 qml-module-qtqml-models2 libusb-1.0-0-dev
    ```

* Make a new folder, clone the pixelpulse library into it from git, and build it!

    ```bash
    mkdir development
    cd development
    git clone https://github.com/analogdevicesinc/Pixelpulse2
    cd pixelpulse2
    cmake -S . -B build -G Ninja
    cmake --build build
    ```

 * Make sure your M1K is plugged into your computer.  The onboard LED should light up when it is connected.  You can double-check by typing ```lsusb```.  You should see something along the lines of ```ID 064b:784c Analog Devices, Inc. (White Mountain DSP)```
 * You should be ready to launch Pixelpulse2. First, go to the directory it was built in:
    
    ```bash
    cd ~/development/pixelpulse2/build
    ```

 * Run Pixelpulse2

    ```bash
    ./pixelpulse2
    ```
