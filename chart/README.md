# RTS VIS

An example charting tool for ADMS applications, using a ZMQ message bus.

Includes a modified copy of the Conditioner code

## Build Requirements

Note: Only has been tested on macOS and Linux.

### MacOS
Requirements:

- MacOS > 10.11 (untested on anything less)
- Xcode command line tools (either from the app or by the `xcode-select` tool)
- Homebrew package manager
- Qt >= 5.10 (either from `brew` or from the installer)
- ZMQ: `brew install zmq`

1. Clone the repo
2. Create a build directory out of tree, enter it
3. Run `qmake path/to/RTSVis.pro` (`brew` will put that in the default path. If you used the Qt installer, you may have to hunt in that install dir for `qmake`)
4. Run `make`

You should now have a shiny new `RTSVis.app` binary ready for use. You may wish to run the deployment tool from Qt on the app to ensure it has all the libraries it needs if you intend to distribute it.

### Linux
Requirements:

- GCC >= 5.6
- Qt >= 5.10 (from your package manager)
- libzmq (from your package manager)

1. Clone the repo
2. Create a build directory out of tree, enter it
3. Run `qmake path/to/RTSVis.pro` or `qmake-qt5 path/to/RTSVis.pro`. Which one depends on your platform and what your package manager uses for their Qt5 install.
4. Run `make`

You should now have a shiny new `RTSVis` binary ready for use.


### Windows

Not recommended. At the moment Windows builds are experimental due to some problems with the ZMQ library.

You can attempt to build natively or by cross compiling from linux or mac:

Requirements:

- GCC >= 5.6
- The MXE cross build environment
- A stiff upper lip and ability to absorb punishment

1. Follow the instructions at the [MXE](https://mxe.cc) site to clone the MXE environment.
2. Use MXE to install `qtbase libzmq`.
3. Watch your disk usage go up and the time slip away.
4. Edit your `PATH` variable to include the binary dirs from MXE.
5. Create a build dir, enter it, and execute the MXE `qmake` binary, which likely has `i686` at the start for your tab-completion pleasure.
6. Run `make`

You _might_ have a shiny new `RTSVis` binary ready for use.

