# `node-gyp` - Node.js native addon build tool

`node-gyp` is a cross-platform command-line tool written in Node.js for compiling
native addon modules for Node.js. It bundles the [gyp](https://gyp.gsrc.io)
project used by the Chromium team and takes away the pain of dealing with the
various differences in build platforms.

Multiple target versions of Node.js are supported (i.e. `0.8`, ..., `4`, `5`, `6`,
etc.), regardless of what version of Node.js is actually installed on your system
(`node-gyp` downloads the necessary development files or headers for the target version).

## Features

 * Easy to use, consistent interface
 * Same commands to build your module on every platform
 * Supports multiple target versions of Node.js

## Installation

You can install with `npm`:

``` bash
$ npm install -g node-gyp
```

You will also need to install:

### On Unix

   * `python` (`v2.7` recommended, `v3.x.x` is __*not*__ supported)
   * `make`
   * A proper C/C++ compiler toolchain, like [GCC](https://gcc.gnu.org)

### On macOS

   * `python` (`v2.7` recommended, `v3.x.x` is __*not*__ supported) (already installed on macOS)
   * [Xcode](https://developer.apple.com/xcode/download/)
     * You also need to install the `Command Line Tools` via Xcode. You can find this under the menu `Xcode -> Preferences -> Locations` (or by running `xcode-select --install` in your Terminal)
       * This step will install `gcc` and the related toolchain containing `make`

### On Windows

#### Option 1

Install all the required tools and configurations using Microsoft's [windows-build-tools](https://github.com/felixrieseberg/windows-build-tools) using `npm install --global --production windows-build-tools` from an elevated PowerShell or CMD.exe (run as Administrator).

#### Option 2

Install tools and configuration manually:
   * Install Visual C++ Build Environment: [Visual Studio Build Tools](https://visualstudio.microsoft.com/thank-you-downloading-visual-studio/?sku=BuildTools)
   (using "Visual C++ build tools" workload) or [Visual Studio 2017 Community](https://visualstudio.microsoft.com/pl/thank-you-downloading-visual-studio/?sku=Community)
   (using the "Desktop development with C++" workload)
   * Install [Python 2.7](https://www.python.org/downloads/) (`v3.x.x` is not supported), and run `npm config set python python2.7` (or see below for further instructions on specifying the proper Python version and path.)
   * Launch cmd, `npm config set msvs_version 2017`

   If the above steps didn't work for you, please visit [Microsoft's Node.js Guidelines for Windows](https://github.com/Microsoft/nodejs-guidelines/blob/master/windows-environment.md#compiling-native-addon-modules) for additional tips.

   To target native ARM64 Node.js on Windows 10 on ARM, add the components "Visual C++ compilers and libraries for ARM64" and "Visual C++ ATL for ARM64".

### Configuring Python Dependency

If you have multiple Python versions installed, you can identify which Python
version `node-gyp` uses by setting the `--python` variable:

``` bash
$ node-gyp --python /path/to/python2.7
```

If `node-gyp` is called by way of `npm`, *and* you have multiple versions of
Python installed, then you can set `npm`'s 'python' config key to the appropriate
value:

``` bash
$ npm config set python /path/to/executable/python2.7
```

## How to Use

To compile your native addon, first go to its root directory:

``` bash
$ cd my_node_addon
```

The next step is to generate the appropriate project build files for the current
platform. Use `configure` for that:

``` bash
$ node-gyp configure
```

Auto-detection fails for Visual C++ Build Tools 2015, so `--msvs_version=2015`
needs to be added (not needed when run by npm as configured above):
``` bash
$ node-gyp configure --msvs_version=2015
```

__Note__: The `configure` step looks for a `binding.gyp` file in the current
directory to process. See below for instructions on creating a `binding.gyp` file.

Now you will have either a `Makefile` (on Unix platforms) or a `vcxproj` file
(on Windows) in the `build/` directory. Next, invoke the `build` command:

``` bash
$ node-gyp build
```

Now you have your compiled `.node` bindings file! The compiled bindings end up
in `build/Debug/` or `build/Release/`, depending on the build mode. At this point,
you can require the `.node` file with Node.js and run your tests!

__Note:__ To create a _Debug_ build of the bindings file, pass the `--debug` (or
`-d`) switch when running either the `configure`, `build` or `rebuild` commands.

## The `binding.gyp` file

A `binding.gyp` file describes the configuration to build your module, in a
JSON-like format. This file gets placed in the root of your package, alongside
`package.json`.

A barebones `gyp` file appropriate for building a Node.js addon could look like:

``` python
{
  "targets": [
    {
      "target_name": "binding",
      "sources": [ "src/binding.cc" ]
    }
  ]
}
```

Some additional resources for addons and writing `gyp` files:

 * ["Going Native" a nodeschool.io tutorial](http://nodeschool.io/#goingnative)
 * ["Hello World" node addon example](https://github.com/nodejs/node/tree/master/test/addons/hello-world)
 * [gyp user documentation](https://gyp.gsrc.io/docs/UserDocumentation.md)
 * [gyp input format reference](https://gyp.gsrc.io/docs/InputFormatReference.md)
 * [*"binding.gyp" files out in the wild* wiki page](https://github.com/nodejs/node-gyp/wiki/%22binding.gyp%22-files-out-in-the-wild)


## Commands

`node-gyp` responds to the following commands:

| **Command**   | **Description**
|:--------------|:---------------------------------------------------------------
| `help`        | Shows the help dialog
| `build`       | Invokes `make`/`msbuild.exe` and builds the native addon
| `clean`       | Removes the `build` directory if it exists
| `configure`   | Generates project build files for the current platform
| `rebuild`     | Runs `clean`, `configure` and `build` all in a row
| `install`     | Installs Node.js header files for the given version
| `list`        | Lists the currently installed Node.js header versions
| `remove`      | Removes the Node.js header files for the given version


## Command Options

`node-gyp` accepts the following command options:

| **Command**                       | **Description**
|:----------------------------------|:------------------------------------------
| `-j n`, `--jobs n`                | Run `make` in parallel. The value `max` will use all available CPU cores
| `--target=v6.2.1`                 | Node.js version to build for (default is `process.version`)
| `--silly`, `--loglevel=silly`     | Log all progress to console
| `--verbose`, `--loglevel=verbose` | Log most progress to console
| `--silent`, `--loglevel=silent`   | Don't log anything to console
| `debug`, `--debug`                | Make Debug build (default is `Release`)
| `--release`, `--no-debug`         | Make Release build
| `-C $dir`, `--directory=$dir`     | Run command in different directory
| `--make=$make`                    | Override `make` command (e.g. `gmake`)
| `--thin=yes`                      | Enable thin static libraries
| `--arch=$arch`                    | Set target architecture (e.g. ia32)
| `--tarball=$path`                 | Get headers from a local tarball
| `--devdir=$path`                  | SDK download directory (default is OS cache directory)
| `--ensure`                        | Don't reinstall headers if already present
| `--dist-url=$url`                 | Download header tarball from custom URL
| `--proxy=$url`                    | Set HTTP proxy for downloading header tarball
| `--cafile=$cafile`                | Override default CA chain (to download tarball)
| `--nodedir=$path`                 | Set the path to the node source code
| `--python=$path`                  | Set path to the Python 2 binary
| `--msvs_version=$version`         | Set Visual Studio version (Windows only)
| `--solution=$solution`            | Set Visual Studio Solution version (Windows only)

## Configuration

### Environment variables

Use the form `npm_config_OPTION_NAME` for any of the command options listed
above (dashes in option names should be replaced by underscores).

For example, to set `devdir` equal to `/tmp/.gyp`, you would:

Run this on Unix:

```bash
$ export npm_config_devdir=/tmp/.gyp
```

Or this on Windows:

```console
> set npm_config_devdir=c:\temp\.gyp
```

### `npm` configuration

Use the form `OPTION_NAME` for any of the command options listed above.

For example, to set `devdir` equal to `/tmp/.gyp`, you would run:

```bash
$ npm config set [--global] devdir /tmp/.gyp
```

**Note:** Configuration set via `npm` will only be used when `node-gyp`
is run via `npm`, not when `node-gyp` is run directly.

## License

`node-gyp` is available under the MIT license. See the [LICENSE
file](LICENSE) for details.
