# TTY

    Stability: 2 - Unstable

The `tty` module houses the `tty.ReadStream` and `tty.WriteStream` classes. In
most cases, you will not need to use this module directly.

When node detects that it is being run inside a TTY context, then `process.stdin`
will be a `tty.ReadStream` instance and `process.stdout` will be
a `tty.WriteStream` instance. The preferred way to check if node is being run in
a TTY context is to check `process.stdout.isTTY`:

    $ node -p -e "Boolean(process.stdout.isTTY)"
    true
    $ node -p -e "Boolean(process.stdout.isTTY)" | cat
    false


## tty.isatty(fd)

Returns `true` or `false` depending on if the `fd` is associated with a
terminal.


## tty.setRawMode(mode)

Deprecated. Use `tty.ReadStream#setRawMode()`
(i.e. `process.stdin.setRawMode()`) instead.


## Class: ReadStream

A `net.Socket` subclass that represents the readable portion of a tty. In normal
circumstances, `process.stdin` will be the only `tty.ReadStream` instance in any
node program (only when `isatty(0)` is true).

### rs.isRaw

A `Boolean` that is initialized to `false`. It represents the current "raw" state
of the `tty.ReadStream` instance.

### rs.setRawMode(mode)

`mode` should be `true` or `false`. This sets the properties of the
`tty.ReadStream` to act either as a raw device or default. `isRaw` will be set
to the resulting mode.


## Class WriteStream

A `net.Socket` subclass that represents the writable portion of a tty. In normal
circumstances, `process.stdout` will be the only `tty.WriteStream` instance
ever created (and only when `isatty(1)` is true).

### ws.columns

A `Number` that gives the number of columns the TTY currently has. This property
gets updated on "resize" events.

### ws.rows

A `Number` that gives the number of rows the TTY currently has. This property
gets updated on "resize" events.

### Event: 'resize'

`function () {}`

Emitted by `refreshSize()` when either of the `columns` or `rows` properties
has changed.

    process.stdout.on('resize', function() {
      console.log('screen size has changed!');
      console.log(process.stdout.columns + 'x' + process.stdout.rows);
    });
