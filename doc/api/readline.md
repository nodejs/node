# Readline

> Stability: 2 - Stable

The `readline` module provides an interface for reading data from a [Readable][]
stream (such as [`process.stdin`]) one line at a time. It can be accessed using:

```js
const readline = require('readline');
```

The following simple example illustrates the basic use of the `readline` module.

```js
const readline = require('readline');

const rl = readline.createInterface({
  input: process.stdin,
  output: process.stdout
});

rl.question('What do you think of Node.js? ', (answer) => {
  // TODO: Log the answer in a database
  console.log('Thank you for your valuable feedback:', answer);

  rl.close();
});
```

*Note* Once this code is invoked, the Node.js application will not
terminate until the `readline.Interface` is closed because the interface
waits for data to be received on the `input` stream.

## Class: Interface
<!-- YAML
added: v0.1.104
-->

Instances of the `readline.Interface` class are constructed using the
`readline.createInterface()` method. Every instance is associated with a
single `input` [Readable][] stream and a single `output` [Writable][] stream.
The `output` stream is used to print prompts for user input that arrives on,
and is read from, the `input` stream.

### Event: 'close'
<!-- YAML
added: v0.1.98
-->

The `'close'` event is emitted when one of the following occur:

* The `rl.close()` method is called and the `readline.Interface` instance has
  relinquished control over the `input` and `output` streams;
* The `input` stream receives its `'end'` event;
* The `input` stream receives `<ctrl>-D` to signal end-of-transmission (EOT);
* The `input` stream receives `<ctrl>-C` to signal `SIGINT` and there is no
  `SIGINT` event listener registered on the `readline.Interface` instance.

The listener function is called without passing any arguments.

The `readline.Interface` instance should be considered to be "finished" once
the `'close'` event is emitted.

### Event: 'line'
<!-- YAML
added: v0.1.98
-->

The `'line'` event is emitted whenever the `input` stream receives an
end-of-line input (`\n`, `\r`, or `\r\n`). This usually occurs when the user
presses the `<Enter>`, or `<Return>` keys.

The listener function is called with a string containing the single line of
received input.

For example:

```js
rl.on('line', (input) => {
  console.log(`Received: ${input}`);
});
```

### Event: 'pause'
<!-- YAML
added: v0.7.5
-->

The `'pause'` event is emitted when one of the following occur:

* The `input` stream is paused.
* The `input` stream is not paused and receives the `SIGCONT` event. (See
  events `SIGTSTP` and `SIGCONT`)

The listener function is called without passing any arguments.

For example:

```js
rl.on('pause', () => {
  console.log('Readline paused.');
});
```

### Event: 'resume'
<!-- YAML
added: v0.7.5
-->

The `'resume'` event is emitted whenever the `input` stream is resumed.

The listener function is called without passing any arguments.

```js
rl.on('resume', () => {
  console.log('Readline resumed.');
});
```

### Event: 'SIGCONT'
<!-- YAML
added: v0.7.5
-->

The `'SIGCONT'` event is emitted when a Node.js process previously moved into
the background using `<ctrl>-Z` (i.e. `SIGTSTP`) is then brought back to the
foreground using `fg(1)`.

If the `input` stream was paused *before* the `SIGSTP` request, this event will
not be emitted.

The listener function is invoked without passing any arguments.

For example:

```js
rl.on('SIGCONT', () => {
  // `prompt` will automatically resume the stream
  rl.prompt();
});
```

*Note*: The `'SIGCONT'` event is _not_ supported on Windows.

### Event: 'SIGINT'
<!-- YAML
added: v0.3.0
-->

The `'SIGINT'` event is emitted whenever the `input` stream receives a
`<ctrl>-C` input, known typically as `SIGINT`. If there are no `'SIGINT'` event
listeners registered when the `input` stream receives a `SIGINT`, the `'pause'`
event will be emitted.

The listener function is invoked without passing any arguments.

For example:

```js
rl.on('SIGINT', () => {
  rl.question('Are you sure you want to exit?', (answer) => {
    if (answer.match(/^y(es)?$/i)) rl.pause();
  });
});
```

### Event: 'SIGTSTP'
<!-- YAML
added: v0.7.5
-->

The `'SIGTSPT'` event is emitted when the `input` stream receives a `<ctrl>-Z`
input, typically known as `SIGTSTP`. If there are no `SIGTSTP` event listeners
registered when the `input` stream receives a `SIGTSTP`, the Node.js process
will be sent to the background.

When the program is resumed using `fg(1)`, the `'pause'` and `SIGCONT` events
will be emitted. These can be used to resume the `input` stream.

The `'pause'` and `'SIGCONT'` events will not be emitted if the `input` was
paused before the process was sent to the background.

The listener function is invoked without passing any arguments.

For example:

```js
rl.on('SIGTSTP', () => {
  // This will override SIGTSTP and prevent the program from going to the
  // background.
  console.log('Caught SIGTSTP.');
});
```

*Note*: The `'SIGTSTP'` event is _not_ supported on Windows.

### rl.close()
<!-- YAML
added: v0.1.98
-->

The `rl.close()` method closes the `readline.Interface` instance and
relinquishes control over the `input` and `output` streams. When called,
the `'close'` event will be emitted.
Closes the `Interface` instance, relinquishing control on the `input` and
`output` streams. The `'close'` event will also be emitted.

### rl.pause()
<!-- YAML
added: v0.3.4
-->

The `rl.pause()` method pauses the `input` stream, allowing it to be resumed
later if necessary.

Calling `rl.pause()` does not immediately pause other events (including
`'line'`) from being emitted by the `readline.Interface` instance.

### rl.prompt([preserveCursor])
<!-- YAML
added: v0.1.98
-->

* `preserveCursor` {boolean} If `true`, prevents the cursor placement from
  being reset to `0`.

The `rl.prompt()` method writes the `readline.Interface` instances configured
`prompt` to a new line in `output` in order to provide a user with a new
location at which to provide input.

When called, `rl.prompt()` will resume the `input` stream if it has been
paused.

If the `readline.Interface` was created with `output` set to `null` or
`undefined` the prompt is not written.

### rl.question(query, callback)
<!-- YAML
added: v0.3.3
-->

* `query` {String} A statement or query to write to `output`, prepended to the
  prompt.
* `callback` {Function} A callback function that is invoked with the user's
  input in response to the `query`.

The `rl.question()` method displays the `query` by writing it to the `output`,
waits for user input to be provided on `input`, then invokes the `callback`
function passing the provided input as the first argument.

When called, `rl.question()` will resume the `input` stream if it has been
paused.

If the `readline.Interface` was created with `output` set to `null` or
`undefined` the `query` is not written.

Example usage:

```js
rl.question('What is your favorite food?', (answer) => {
  console.log(`Oh, so your favorite food is ${answer}`);
});
```

*Note*: The `callback` function passed to `rl.question()` does not follow the
typical pattern of accepting an `Error` object or `null` as the first argument.
The `callback` is called with the provided answer as the only argument.

### rl.resume()
<!-- YAML
added: v0.3.4
-->

The `rl.resume()` method resumes the `input` stream if it has been paused.

### rl.setPrompt(prompt)
<!-- YAML
added: v0.1.98
-->

* `prompt` {String}

The `rl.setPrompt()` method sets the prompt that will be written to `output`
whenever `rl.prompt()` is called.

### rl.write(data[, key])
<!-- YAML
added: v0.1.98
-->

* `data` {String}
* `key` {Object}
  * `ctrl` {boolean} `true` to indicate the `<ctrl>` key.
  * `meta` {boolean} `true` to indicate the `<Meta>` key.
  * `shift` {boolean} `true` to indicate the `<Shift>` key.
  * `name` {String} The name of the a key.

The `rl.write()` method will write either `data` or a key sequence  identified
by `key` to the `output`. The `key` argument is supported only if `output` is
a [TTY][] text terminal.

If `key` is specified, `data` is ignored.

When called, `rl.write()` will resume the `input` stream if it has been
paused.

If the `readline.Interface` was created with `output` set to `null` or
`undefined` the `data` and `key` are not written.

For example:

```js
rl.write('Delete this!');
// Simulate Ctrl+u to delete the line written previously
rl.write(null, {ctrl: true, name: 'u'});
```

## readline.clearLine(stream, dir)
<!-- YAML
added: v0.7.7
-->

* `stream` {Writable}
* `dir` {number}
  * `-1` - to the left from cursor
  * `1` - to the right from cursor
  * `0` - the entire line

The `readline.clearLine()` method clears current line of given [TTY][] stream
in a specified direction identified by `dir`.


## readline.clearScreenDown(stream)
<!-- YAML
added: v0.7.7
-->

* `stream` {Writable}

The `readline.clearScreenDown()` method clears the given [TTY][] stream from
the current position of the cursor down.

## readline.createInterface(options)
<!-- YAML
added: v0.1.98
-->

* `options` {Object}
  * `input` {Readable} The [Readable][] stream to listen to. This option is
    *required*.
  * `output` {Writable} The [Writable][] stream to write readline data to.
  * `completer` {Function} An optional function used for Tab autocompletion.
  * `terminal` {boolean} `true` if the `input` and `output` streams should be
    treated like a TTY, and have ANSI/VT100 escape codes written to it.
    Defaults to checking `isTTY` on the `output` stream upon instantiation.
  * `historySize` {number} maximum number of history lines retained. To disable
    the history set this value to `0`. Defaults to `30`. This option makes sense
    only if `terminal` is set to `true` by the user or by an internal `output`
    check, otherwise the history caching mechanism is not initialized at all.
  * `prompt` - the prompt string to use. Default: `'> '`

The `readline.createInterface()` method creates a new `readline.Interface`
instance.

For example:

```js
const readline = require('readline');
const rl = readline.createInterface({
  input: process.stdin,
  output: process.stdout
});
```

Once the `readline.Interface` instance is created, the most common case is to
listen for the `'line'` event:

```js
rl.on('line', (line) => {
  console.log(`Received: ${line}`);
});
```

If `terminal` is `true` for this instance then the `output` stream will get
the best compatibility if it defines an `output.columns` property and emits
a `'resize'` event on the `output` if or when the columns ever change
([`process.stdout`][] does this automatically when it is a TTY).

### Use of the `completer` Function

When called, the `completer` function is provided the current line entered by
the user, and is expected to return an Array with 2 entries:

* An Array with matching entries for the completion.
* The substring that was used for the matching.

For instance: `[[substr1, substr2, ...], originalsubstring]`.

```js
function completer(line) {
  var completions = '.help .error .exit .quit .q'.split(' ');
  var hits = completions.filter((c) => { return c.indexOf(line) == 0 });
  // show all completions if none found
  return [hits.length ? hits : completions, line];
}
```

The `completer` function can be called asynchronously if it accepts two
arguments:

```js
function completer(linePartial, callback) {
  callback(null, [['123'], linePartial]);
}
```

## readline.cursorTo(stream, x, y)
<!-- YAML
added: v0.7.7
-->

* `stream` {Writable}
* `x` {number}
* `y` {number}

The `readline.cursorTo()` method moves cursor to the specified position in a
given [TTY][] `stream`.

## readline.emitKeypressEvents(stream[, interface])
<!-- YAML
added: v0.7.7
-->

* `stream` {Readable}
* `interface` {readline.Interface}

The `readline.emitKeypressEvents()` method causes the given [Writable][]
`stream` to begin emitting `'keypress'` events corresponding to received input.

Optionally, `interface` specifies a `readline.Interface` instance for which
autocompletion is disabled when copy-pasted input is detected.

If the `stream` is a [TTY][], then it must be in raw mode.

```js
readline.emitKeypressEvents(process.stdin);
if (process.stdin.isTTY)
  process.stdin.setRawMode(true);
```

## readline.moveCursor(stream, dx, dy)
<!-- YAML
added: v0.7.7
-->

* `stream` {Writable}
* `dx` {number}
* `dy` {Number}

The `readline.moveCursor()` method moves the cursor *relative* to its current
position in a given [TTY][] `stream`.


## Example: Tiny CLI

The following example illustrates the use of `readline.Interface` class to
implement a small command-line interface:

```js
const readline = require('readline');
const rl = readline.createInterface({
  input: process.stdin,
  output: process.stdout,
  prompt: 'OHAI> '
});

rl.prompt();

rl.on('line', (line) => {
  switch(line.trim()) {
    case 'hello':
      console.log('world!');
      break;
    default:
      console.log(`Say what? I might have heard '${line.trim()}'`);
      break;
  }
  rl.prompt();
}).on('close', () => {
  console.log('Have a great day!');
  process.exit(0);
});
```

## Example: Read File Stream Line-by-Line

A common use case for `readline` is to consume input from a filesystem
[Readable][] stream one line at a time, as illustrated in the following
example:

```js
const readline = require('readline');
const fs = require('fs');

const rl = readline.createInterface({
  input: fs.createReadStream('sample.txt')
});

rl.on('line', (line) => {
  console.log('Line from file:', line);
});
```

[`process.stdin`]: process.html#process_process_stdin
[`process.stdout`]: process.html#process_process_stdout
[Writable]: stream.html
[Readable]: stream.html
[TTY]: tty.html
