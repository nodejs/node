Evented I/O for V8 javascript.
===

### To build:

Prerequisites (Unix only):

    * GCC 4.2 or newer
    * Python 2.6 or 2.7
    * GNU Make 3.81 or newer
    * libexecinfo (FreeBSD and OpenBSD only)

Unix/Macintosh:

    ./configure
    make
    make install

If your python binary is in a non-standard location or has a
non-standard name, run the following instead:

    export PYTHON=/path/to/python
    $PYTHON ./configure
    make
    make install

Prerequisites (Windows only):

    * Python 2.6 or 2.7
    * Visual Studio 2010 or 2012

Windows:

    vcbuild nosign

You can download pre-built binaries for various operating systems from
[http://nodejs.org/download/](http://nodejs.org/download/).  The Windows
and OS X installers will prompt you for the location to install to.
The tarballs are self-contained; you can extract them to a local directory
with:

    tar xzf /path/to/node-<version>-<platform>-<arch>.tar.gz

Or system-wide with:

    cd /usr/local && tar --strip-components 1 -xzf \
                         /path/to/node-<version>-<platform>-<arch>.tar.gz

### To run the tests:

Unix/Macintosh:

    make test

Windows:

    vcbuild test

### To build the documentation:

    make doc

### To read the documentation:

    man doc/node.1

Resources for Newcomers
---
  - [The Wiki](https://github.com/joyent/node/wiki)
  - [nodejs.org](http://nodejs.org/)
  - [how to install node.js and npm (node package manager)](http://www.joyent.com/blog/installing-node-and-npm/)
  - [list of modules](https://github.com/joyent/node/wiki/modules)
  - [searching the npm registry](http://npmjs.org/)
  - [list of companies and projects using node](https://github.com/joyent/node/wiki/Projects,-Applications,-and-Companies-Using-Node)
  - [node.js mailing list](http://groups.google.com/group/nodejs)
  - irc chatroom, [#node.js on freenode.net](http://webchat.freenode.net?channels=node.js&uio=d4)
  - [community](https://github.com/joyent/node/wiki/Community)
  - [contributing](https://github.com/joyent/node/wiki/Contributing)
  - [big list of all the helpful wiki pages](https://github.com/joyent/node/wiki/_pages)
