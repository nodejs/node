# read-cmd-shim

Figure out what a [`cmd-shim`](https://github.com/ForbesLindesay/cmd-shim)
is pointing at.  This acts as the equivalent of
[`fs.readlink`](https://nodejs.org/api/fs.html#fs_fs_readlink_path_callback).

### Usage

```
var readCmdShim = require('read-cmd-shim')

readCmdShim('/path/to/shim.cmd', function (er, destination) {
  …
})

var destination = readCmdShim.sync('/path/to/shim.cmd')

### readCmdShim(path, callback)

Reads the `cmd-shim` located at `path` and calls back with the _relative_
path that the shim points at. Consider this as roughly the equivalent of
`fs.readlink`.

This can read both `.cmd` style that are run by the Windows Command Prompt
and Powershell, and the kind without any extension that are used by Cygwin.

This can return errors that `fs.readFile` returns, except that they'll
include a stack trace from where `readCmdShim` was called.  Plus it can
return a special `ENOTASHIM` exception, when it can't find a cmd-shim in the
file referenced by `path`.  This should only happen if you pass in a
non-command shim.


### readCmdShim.sync(path)

Same as above but synchronous. Errors are thrown.
