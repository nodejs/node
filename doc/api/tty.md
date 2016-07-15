# TTY

> Stability: 2 - Stable

The `tty` module provides the `tty.ReadStream` and `tty.WriteStream` classes.
In most cases, it will not be necessary or possible to use this module directly.
However, it can be accessed using:

```js
const tty = require('tty');
```

When Node.js detects that it is being run inside a text terminal ("TTY")
context, the `process.stdin` will, by default, be initialized as an instance of
`tty.ReadStream` and both `process.stdout` and `process.stderr` will, by
default be instances of `tty.WriteStream`. The preferred method of determining
whether Node.js is being run within a TTY context is to check that the value of
the `process.stdout.isTTY` property is `true`:

```sh
$ node -p -e "Boolean(process.stdout.isTTY)"
true
$ node -p -e "Boolean(process.stdout.isTTY)" | cat
false
```

In most cases, there should be little to no reason for an application to
create instances of the `tty.ReadStream` and `tty.WriteStream` classes.

## Class: tty.ReadStream
<!-- YAML
added: v0.5.8
-->

The `tty.ReadStream` class is a subclass of `net.Socket` that represents the
readable side of a TTY. In normal circumstances `process.stdin` will be the
only `tty.ReadStream` instance in a Node.js process and there should be no
reason to create additional instances.

### readStream.isRaw
<!-- YAML
added: v0.7.7
-->

A `boolean` that is `true` if the TTY is currently configured to operate as a
raw device. Defaults to `false`.

### readStream.setRawMode(mode)
<!-- YAML
added: v0.7.7
-->

* `mode` {boolean} If `true`, configures the `tty.ReadStream` to operate as a
  raw device. If `false`, configures the `tty.ReadStream` to operate in its
  default mode. The `readStream.isRaw` property will be set to the resulting
  mode.

## Class: tty.WriteStream
<!-- YAML
added: v0.5.8
-->

The `tty.WriteStream` class is a subclass of `net.Socket` that represents the
writable side of a TTY. In normal circumstances, `process.stdout` and
`process.stderr` will be the only `tty.WriteStream` instances created for a
Node.js process and there should be no reason to create additional instances.

### Event: 'resize'
<!-- YAML
added: v0.7.7
-->

The `'resize'` event is emitted whenever either of the `writeStream.columns`
or `writeStream.rows` properties have changed. No arguments are passed to the
listener callback when called.

```js
process.stdout.on('resize', () => {
  console.log('screen size has changed!');
  console.log(`${process.stdout.columns}x${process.stdout.rows}`);
});
```

### writeStream.columns
<!-- YAML
added: v0.7.7
-->

A `number` specifying the number of columns the TTY currently has. This property
is updated whenever the `'resize'` event is emitted.

### writeStream.rows
<!-- YAML
added: v0.7.7
-->

A `number` specifying the number of rows the TTY currently has. This property
is updated whenever the `'resize'` event is emitted.

## tty.isatty(fd)
<!-- YAML
added: v0.5.8
-->

* `fd` {number} A numeric file descriptor

The `tty.isatty()` method returns `true` if the given `fd` is associated with
a TTY and `false` if is not.
