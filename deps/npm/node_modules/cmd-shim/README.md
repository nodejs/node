# cmd-shim

The cmd-shim used in npm to create executable scripts on Windows,
since symlinks are not suitable for this purpose there.

On Unix systems, you should use a symbolic link instead.

[![Build Status](https://img.shields.io/travis/npm/cmd-shim/master.svg)](https://travis-ci.org/npm/cmd-shim)
[![Dependency Status](https://img.shields.io/david/npm/cmd-shim.svg)](https://david-dm.org/npm/cmd-shim)
[![npm version](https://img.shields.io/npm/v/cmd-shim.svg)](https://www.npmjs.com/package/cmd-shim)

## Installation

```
npm install cmd-shim
```

## API

### cmdShim(from, to) -> Promise

Create a cmd shim at `to` for the command line program at `from`.
e.g.

```javascript
var cmdShim = require('cmd-shim');
cmdShim(__dirname + '/cli.js', '/usr/bin/command-name').then(() => {
  // shims are created!
})
```

### cmdShim.ifExists(from, to) -> Promise

The same as above, but will just continue if the file does not exist.
