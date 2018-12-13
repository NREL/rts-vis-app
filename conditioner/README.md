# Conditioner

A tool for reading ADMS data from ZMQ, resampling, and exporting over a websocket.

This is intended to be a command line tool.

## Usage

### Command Line Options

```
Usage: ./conditioner [options] experiment
ADMS data conditioning server

Options:
  -h, --help     Displays this help.
  -v, --version  Displays version information.
  -s <host>      ZMQ host to query. Default is tcp://my_relay:5020
  -p <port>      Websocket port to publish on. Default is 50012
  -r <rate>      Message rate, given as messages per second. Default is 10
                 messages/sec

Arguments:
  experiment     Json file describing the experimental data setup.

```

### Websocket Usage

Connect to the server's websocket. You will immediately get a message along the
text channel, which is a Json object:

```json
{
    "sample_rate_msec" : 10,
    "mapping" : {
        "{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}" : 0,
        "{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}" : 1,
        ...
    }
}
```

This object helps map experiment variable ids to the state vector that is provided along the binary message channel of the websocket.

The binary format is host endian, and contains all the variables sampled at this point in time in the following packed format:

```
[float64 timestamp][uint64_t number_of_vars][float32 * number_of_vars]
```

Thus to look up a variable with a given UUID, you would consult the mapping table for the index, and use the index to look into this binary variable list.

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
3. Run `qmake path/to/conditioner.pro` (`brew` will put that in the default path. If you used the Qt installer, you may have to hunt in that install dir for `qmake`)
4. Run `make`

You should now have a shiny new `conditioner` binary ready for use.

### Linux
Requirements:

- GCC >= 5.6
- Qt >= 5.10 (from your package manager)
- libzmq (from your package manager)

1. Clone the repo
2. Create a build directory out of tree, enter it
3. Run `qmake path/to/conditioner.pro` or `qmake-qt5 path/to/conditioner.pro`. Which one depends on your platform and what your package manager uses for their Qt5 install.
4. Run `make`

You should now have a shiny new `conditioner` binary ready for use.


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

You _might_ have a shiny new `conditioner` binary ready for use.
