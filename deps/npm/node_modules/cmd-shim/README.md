# cmd-shim

The cmd-shim used in npm to create executable scripts on Windows,
since symlinks are not suitable for this purpose there.

On Unix systems, you should use a symbolic link instead.

## Installation

```
npm install cmd-shim
```

## API

### cmdShim(from, to, cb)

Create a cmd shim at `to` for the command line program at `from`.
e.g.

```javascript
var cmdShim = require('cmd-shim');
cmdShim(__dirname + '/cli.js', '/usr/bin/command-name', function (err) {
  if (err) throw err;
});
```

### cmdShim.ifExists(from, to, cb)

The same as above, but will just continue if the file does not exist.
Source:

```javascript
function cmdShimIfExists (from, to, cb) {
  fs.stat(from, function (er) {
    if (er) return cb()
    cmdShim(from, to, cb)
  })
}
```
