# TTY

<!--introduced_in=v0.10.0-->

> Stability: 2 - Stable

The `tty` module provides the `tty.ReadStream` and `tty.WriteStream` classes.
In most cases, it will not be necessary or possible to use this module directly.
However, it can be accessed using:

```js
const tty = require('tty');
```

When Node.js detects that it is being run with a text terminal ("TTY")
attached, [`process.stdin`][] will, by default, be initialized as an instance of
`tty.ReadStream` and both [`process.stdout`][] and [`process.stderr`][] will, by
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
manually create instances of the `tty.ReadStream` and `tty.WriteStream`
classes.

## Class: tty.ReadStream
<!-- YAML
added: v0.5.8
-->

The `tty.ReadStream` class is a subclass of [`net.Socket`][] that represents the
readable side of a TTY. In normal circumstances [`process.stdin`][] will be the
only `tty.ReadStream` instance in a Node.js process and there should be no
reason to create additional instances.

### readStream.isRaw
<!-- YAML
added: v0.7.7
-->

A `boolean` that is `true` if the TTY is currently configured to operate as a
raw device. Defaults to `false`.

### readStream.isTTY
<!-- YAML
added: v0.5.8
-->

A `boolean` that is always `true` for `tty.ReadStream` instances.

### readStream.setRawMode(mode)
<!-- YAML
added: v0.7.7
-->

* `mode` {boolean} If `true`, configures the `tty.ReadStream` to operate as a
  raw device. If `false`, configures the `tty.ReadStream` to operate in its
  default mode. The `readStream.isRaw` property will be set to the resulting
  mode.
* Returns: {this} - the read stream instance.

Allows configuration of `tty.ReadStream` so that it operates as a raw device.

When in raw mode, input is always available character-by-character, not
including modifiers. Additionally, all special processing of characters by the
terminal is disabled, including echoing input characters.
Note that `CTRL`+`C` will no longer cause a `SIGINT` when in this mode.

## Class: tty.WriteStream
<!-- YAML
added: v0.5.8
-->

The `tty.WriteStream` class is a subclass of [`net.Socket`][] that represents
the writable side of a TTY. In normal circumstances, [`process.stdout`][] and
[`process.stderr`][] will be the only `tty.WriteStream` instances created for a
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

### writeStream.clearLine(dir)
<!-- YAML
added: v0.7.7
-->

* `dir` {number}
  * `-1` - to the left from cursor
  * `1` - to the right from cursor
  * `0` - the entire line

`writeStream.clearLine()` clears the current line of this `WriteStream` in a
direction identified by `dir`.

### writeStream.clearScreenDown()
<!-- YAML
added: v0.7.7
-->

`writeStream.clearScreenDown()` clears this `WriteStream` from the current
cursor down.

### writeStream.columns
<!-- YAML
added: v0.7.7
-->

A `number` specifying the number of columns the TTY currently has. This property
is updated whenever the `'resize'` event is emitted.

### writeStream.cursorTo(x, y)
<!-- YAML
added: v0.7.7
-->

* `x` {number}
* `y` {number}

`writeStream.cursorTo()` moves this `WriteStream`'s cursor to the specified
position.

### writeStream.getColorDepth([env])
<!-- YAML
added: v9.9.0
-->

* `env` {Object} An object containing the environment variables to check.
  **Default:** `process.env`.
* Returns: {number}

Returns:
* `1` for 2,
* `4` for 16,
* `8` for 256,
* `24` for 16,777,216
colors supported.

Use this to determine what colors the terminal supports. Due to the nature of
colors in terminals it is possible to either have false positives or false
negatives. It depends on process information and the environment variables that
may lie about what terminal is used.
To enforce a specific behavior without relying on `process.env` it is possible
to pass in an object with different settings.

Use the `NODE_DISABLE_COLORS` environment variable to enforce this function to
always return 1.

### writeStream.getWindowSize()
<!-- YAML
added: v0.7.7
-->
* Returns: {number[]}

`writeStream.getWindowSize()` returns the size of the [TTY](tty.html)
corresponding to this `WriteStream`. The array is of the type
`[numColumns, numRows]` where `numColumns` and `numRows` represent the number
of columns and rows in the corresponding [TTY](tty.html).

### writeStream.isTTY
<!-- YAML
added: v0.5.8
-->

A `boolean` that is always `true`.

### writeStream.moveCursor(dx, dy)
<!-- YAML
added: v0.7.7
-->

* `dx` {number}
* `dy` {number}

`writeStream.moveCursor()` moves this `WriteStream`'s cursor *relative* to its
current position.

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
a TTY and `false` if it is not, including whenever `fd` is not a non-negative
integer.

[`net.Socket`]: net.html#net_class_net_socket
[`process.stderr`]: process.html#process_process_stderr
[`process.stdin`]: process.html#process_process_stdin
[`process.stdout`]: process.html#process_process_stdout
