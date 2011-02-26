## TTY

Use `require('tty')` to access this module.

Example:

    var tty = require('tty');
    tty.setRawMode(true);
    process.stdin.resume();
    process.stdin.on('keypress', function(char, key) {
      if (key && key.ctrl && key.name == 'c') {
        console.log('graceful exit');
        process.exit()
      }
    });



### tty.open(path, args=[])

Spawns a new process with the executable pointed to by `path` as the session
leader to a new pseudo terminal.

Returns an array `[slaveFD, childProcess]`. `slaveFD` is the file descriptor
of the slave end of the pseudo terminal. `childProcess` is a child process
object.


### tty.isatty(fd)

Returns `true` or `false` depending on if the `fd` is associated with a
terminal.


### tty.setRawMode(mode)

`mode` should be `true` or `false`. This sets the properties of the current
process's stdin fd to act either as a raw device or default.


### tty.setWindowSize(fd, row, col)

`ioctl`s the window size settings to the file descriptor.


### tty.getWindowSize(fd)

Returns `[row, col]` for the TTY associated with the file descriptor.


