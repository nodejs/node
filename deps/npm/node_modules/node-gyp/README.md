node-gyp
=========
### Node.js native addon build tool

`node-gyp` is a cross-platform command-line tool written in Node.js for compiling
native addon modules for Node.js.  It bundles the [gyp](https://code.google.com/p/gyp/)
project used by the Chromium team and takes away the pain of dealing with the
various differences in build platforms. It is the replacement to the `node-waf`
program which is removed for node `v0.8`. If you have a native addon for node that
still has a `wscript` file, then you should definitely add a `binding.gyp` file
to support the latest versions of node.

Multiple target versions of node are supported (i.e. `0.8`, `0.9`, `0.10`, ..., `1.0`,
etc.), regardless of what version of node is actually installed on your system
(`node-gyp` downloads the necessary development files for the target version).

#### Features:

 * Easy to use, consistent interface
 * Same commands to build your module on every platform
 * Supports multiple target versions of Node


Installation
------------

You can install with `npm`:

``` bash
$ npm install -g node-gyp
```

You will also need to install:

  * On Unix:
    * `python` (`v2.7` recommended, `v3.x.x` is __*not*__ supported)
    * `make`
    * A proper C/C++ compiler toolchain, like [GCC](https://gcc.gnu.org)
  * On Mac OS X:
    * `python` (`v2.7` recommended, `v3.x.x` is __*not*__ supported) (already installed on Mac OS X)
    * [Xcode](https://developer.apple.com/xcode/downloads/)
      * You also need to install the `Command Line Tools` via Xcode. You can find this under the menu `Xcode -> Preferences -> Downloads`
      * This step will install `gcc` and the related toolchain containing `make`
  * On Windows:
    * [Python][windows-python] ([`v2.7.3`][windows-python-v2.7.3] recommended, `v3.x.x` is __*not*__ supported)
      * Make sure that you have a PYTHON environment variable, and it is set to drive:\path\to\python.exe not to a folder
    * Windows XP/Vista/7:
      * Microsoft Visual Studio C++ 2013 ([Express][msvc2013] version works well)
        * If the install fails, try uninstalling any C++ 2010 x64&x86 Redistributable that you have installed first
        * If you get errors that the 64-bit compilers are not installed you may also need the [compiler update for the Windows SDK 7.1]
    * Windows 7/8:
      * Microsoft Visual Studio C++ 2013 for Windows Desktop ([Express][msvc2013] version works well)
    * All Windows Versions
      * For 64-bit builds of node and native modules you will _**also**_ need the [Windows 7 64-bit SDK][win7sdk]
      * You may need to run one of the following commands if your build complains about WindowsSDKDir not being set, and you are sure you have already installed the SDK:

```
call "C:\Program Files\Microsoft SDKs\Windows\v7.1\bin\Setenv.cmd" /Release /x86
call "C:\Program Files\Microsoft SDKs\Windows\v7.1\bin\Setenv.cmd" /Release /x64
```

If you have multiple Python versions installed, you can identify which Python
version `node-gyp` uses by setting the '--python' variable:

``` bash
$ node-gyp --python /path/to/python2.7
```

If `node-gyp` is called by way of `npm` *and* you have multiple versions of
Python installed, then you can set `npm`'s 'python' config key to the appropriate
value:

``` bash
$ npm config set python /path/to/executable/python2.7
```

Note that OS X is just a flavour of Unix and so needs `python`, `make`, and C/C++.
An easy way to obtain these is to install XCode from Apple,
and then use it to install the command line tools (under Preferences -> Downloads).

How to Use
----------

To compile your native addon, first go to its root directory:

``` bash
$ cd my_node_addon
```

The next step is to generate the appropriate project build files for the current
platform. Use `configure` for that:

``` bash
$ node-gyp configure
```

__Note__: The `configure` step looks for the `binding.gyp` file in the current
directory to process. See below for instructions on creating the `binding.gyp` file.

Now you will have either a `Makefile` (on Unix platforms) or a `vcxproj` file
(on Windows) in the `build/` directory. Next invoke the `build` command:

``` bash
$ node-gyp build
```

Now you have your compiled `.node` bindings file! The compiled bindings end up
in `build/Debug/` or `build/Release/`, depending on the build mode. At this point
you can require the `.node` file with Node and run your tests!

__Note:__ To create a _Debug_ build of the bindings file, pass the `--debug` (or
`-d`) switch when running either the `configure`, `build` or `rebuild` command.


The "binding.gyp" file
----------------------

Previously when node had `node-waf` you had to write a `wscript` file. The
replacement for that is the `binding.gyp` file, which describes the configuration
to build your module in a JSON-like format. This file gets placed in the root of
your package, alongside the `package.json` file.

A barebones `gyp` file appropriate for building a node addon looks like:

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
 * ["Hello World" node addon example](https://github.com/joyent/node/tree/master/test/addons/hello-world)
 * [gyp user documentation](https://chromium.googlesource.com/external/gyp/+/master/docs/UserDocumentation.md)
 * [gyp input format reference](https://chromium.googlesource.com/external/gyp/+/master/docs/InputFormatReference.md)
 * [*"binding.gyp" files out in the wild* wiki page](https://github.com/TooTallNate/node-gyp/wiki/%22binding.gyp%22-files-out-in-the-wild)


Commands
--------

`node-gyp` responds to the following commands:

| **Command**   | **Description**
|:--------------|:---------------------------------------------------------------
| `build`       | Invokes `make`/`msbuild.exe` and builds the native addon
| `clean`       | Removes the `build` directory if it exists
| `configure`   | Generates project build files for the current platform
| `rebuild`     | Runs `clean`, `configure` and `build` all in a row
| `install`     | Installs node development header files for the given version
| `list`        | Lists the currently installed node development file versions
| `remove`      | Removes the node development header files for the given version


License
-------

(The MIT License)

Copyright (c) 2012 Nathan Rajlich &lt;nathan@tootallnate.net&gt;

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
'Software'), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


[windows-python]: http://www.python.org/getit/windows
[windows-python-v2.7.3]: http://www.python.org/download/releases/2.7.3#download
[msvc2013]: http://www.visualstudio.com/en-us/downloads/download-visual-studio-vs
[win7sdk]: http://www.microsoft.com/en-us/download/details.aspx?id=8279
[compiler update for the Windows SDK 7.1]: http://www.microsoft.com/en-us/download/details.aspx?id=4422
