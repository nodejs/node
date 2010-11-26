## TTY

Use `require('tty')` to access this module.


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

`mode` should be `true` or `false`. This sets the properies of the current
process's stdin fd to act either as a raw device or default.


### tty.getColumns()

Returns the number of columns associated with the current process's TTY.

Note that each time this number is changed the process receives a `SIGWINCH`
signal. So you can keep a cache of it like this:

    var columns = tty.getColumns();
    process.on('SIGWINCH', function() {
      columns = tty.getColumns();
    });


