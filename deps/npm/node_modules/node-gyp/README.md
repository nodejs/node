node-gyp
=========
### Node.js native addon build tool

`node-gyp` is a cross-platform command-line tool written in Node.js for compiling
native addon modules for Node.js, which takes away the pain of dealing with the
various differences in build platforms. It is the replacement to the `node-waf`
program which is removed for node `v0.8`. If you have a native addon for node that
still has a `wscript` file, then you should definitely add a `binding.gyp` file
to support the latest versions of node.

Multiple target versions of node are supported (i.e. `0.6`, `0.7`,..., `1.0`,
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
    * `python`
    * `make`
    * A proper C/C++ compiler toolchain, like GCC
  * On Windows:
    * [Python][windows-python] ([`v2.7.2`][windows-python-v2.7.2] recommended, `v3.x.x` not yet supported)
    * Microsoft Visual C++ ([Express][msvc] version works well)
      * For 64-bit builds of node and native modules you will _also_ need the [Windows 7 64-bit SDK][win7sdk]

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
directory to processs. See below for instructions on creating the `binding.gyp` file.

Now you will have either a `Makefile` (on Unix platforms) or a `vcxproj` file
(on Windows) in the `build/` directory. Next invoke the `build` command:

``` bash
$ node-gyp build
```

Now you have your compiled `.node` bindings file! The compiled bindings end up
in `build/Debug/` or `build/Release/`, depending on the build mode. At this point
you can require the `.node` file with Node and run your tests!

__Note:__ To create a _Debug_ build of the bindings file, pass the `--debug` (or
`-d`) switch when running the either `configure` or `build` command.


The "binding.gyp" file
----------------------

Previously when node had `node-waf` you had to write a `wscript` file. The
replacement for that is the `binding.gyp` file, which describes the configuration
to build your module in a JSON-like format. This file gets placed in the root of
your package, alongside the `package.json` file.

A barebones `gyp` file appropriate for building a node addon looks like:

``` json
{
  "targets": [
    {
      "target_name": "binding",
      "sources": [ "src/binding.cc" ]
    }
  ]
}
```

Some additional resources for writing `gyp` files:

 * ["Hello World" node addon example](https://github.com/joyent/node/tree/master/test/addons/hello-world)
 * [gyp user documentation](http://code.google.com/p/gyp/wiki/GypUserDocumentation)
 * [gyp input format reference](http://code.google.com/p/gyp/wiki/InputFormatReference)
 * ['"binding.gyp" files out in the wild' wiki page](https://github.com/TooTallNate/node-gyp/wiki/%22binding.gyp%22-files-out-in-the-wild)


Commands
--------

`node-gyp` responds to the following commands:

 * `build` - Invokes `make`/`msbuild.exe` and builds the native addon
 * `clean` - Removes any generated project files and the `out` dir
 * `configure` - Generates project build files for the current platform
 * `rebuild` - Runs "clean", "configure" and "build" all at once
 * `install` - Installs node development files for the given version.
 * `list` - Lists the currently installed node development file versions
 * `remove` - Removes a node development files for a given version


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
[windows-python-v2.7.2]: http://www.python.org/download/releases/2.7.2#download
[msvc]: http://www.microsoft.com/visualstudio/en-us/products/2010-editions/visual-cpp-express
[win7sdk]: http://www.microsoft.com/download/en/details.aspx?displayLang=en&id=8279
