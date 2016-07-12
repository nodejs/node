# TTY

    Stability: 2 - Stable

The `tty` module houses the `tty.ReadStream` and `tty.WriteStream` classes. In
most cases, you will not need to use this module directly.

When Node.js detects that it is being run inside a TTY context, then `process.stdin`
will be a `tty.ReadStream` instance and `process.stdout` will be
a `tty.WriteStream` instance. The preferred way to check if Node.js is being run
in a TTY context is to check `process.stdout.isTTY`:

```
$ node -p -e "Boolean(process.stdout.isTTY)"
true
$ node -p -e "Boolean(process.stdout.isTTY)" | cat
false
```

## Class: ReadStream
<!-- YAML
added: v0.5.8
-->

A `net.Socket` subclass that represents the readable portion of a tty. In normal
circumstances, `process.stdin` will be the only `tty.ReadStream` instance in any
Node.js program (only when `isatty(0)` is true).

### rs.isRaw
<!-- YAML
added: v0.7.7
-->

A `Boolean` that is initialized to `false`. It represents the current "raw" state
of the `tty.ReadStream` instance.

### rs.setRawMode(mode)
<!-- YAML
added: v0.7.7
-->

`mode` should be `true` or `false`. This sets the properties of the
`tty.ReadStream` to act either as a raw device or default. `isRaw` will be set
to the resulting mode.

## Class: WriteStream
<!-- YAML
added: v0.5.8
-->

A `net.Socket` subclass that represents the writable portion of a tty. In normal
circumstances, `process.stdout` will be the only `tty.WriteStream` instance
ever created (and only when `isatty(1)` is true).

### Event: 'resize'
<!-- YAML
added: v0.7.7
-->

`function () {}`

Emitted by `refreshSize()` when either of the `columns` or `rows` properties
has changed.

```js
process.stdout.on('resize', () => {
  console.log('screen size has changed!');
  console.log(`${process.stdout.columns}x${process.stdout.rows}`);
});
```

### ws.columns
<!-- YAML
added: v0.7.7
-->

A `Number` that gives the number of columns the TTY currently has. This property
gets updated on `'resize'` events.

### ws.rows
<!-- YAML
added: v0.7.7
-->

A `Number` that gives the number of rows the TTY currently has. This property
gets updated on `'resize'` events.

## tty.isatty(fd)
<!-- YAML
added: v0.5.8
-->

Returns `true` or `false` depending on if the `fd` is associated with a
terminal.

## tty.setRawMode(mode)

    Stability: 0 - Deprecated: Use [tty.ReadStream#setRawMode][] (i.e. process.stdin.setRawMode) instead.

[tty.ReadStream#setRawMode]: #tty_rs_setrawmode_mode
