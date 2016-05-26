# Readline

    Stability: 2 - Stable

To use this module, do `require('readline')`. Readline allows reading of a
stream (such as [`process.stdin`][]) on a line-by-line basis.

Note that once you've invoked this module, your Node.js program will not
terminate until you've closed the interface. Here's how to allow your
program to gracefully exit:

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

## Class: Interface
<!-- YAML
added: v0.1.104
-->

The class that represents a readline interface with an input and output
stream.

### rl.close()
<!-- YAML
added: v0.1.98
-->

Closes the `Interface` instance, relinquishing control on the `input` and
`output` streams. The `'close'` event will also be emitted.

### rl.pause()
<!-- YAML
added: v0.3.4
-->

Pauses the readline `input` stream, allowing it to be resumed later if needed.

Note that this doesn't immediately pause the stream of events. Several events may
be emitted after calling `pause`, including `line`.

### rl.prompt([preserveCursor])
<!-- YAML
added: v0.1.98
-->

Readies readline for input from the user, putting the current `setPrompt`
options on a new line, giving the user a new spot to write. Set `preserveCursor`
to `true` to prevent the cursor placement being reset to `0`.

This will also resume the `input` stream used with `createInterface` if it has
been paused.

If `output` is set to `null` or `undefined` when calling `createInterface`, the
prompt is not written.

### rl.question(query, callback)
<!-- YAML
added: v0.3.3
-->

Prepends the prompt with `query` and invokes `callback` with the user's
response. Displays the query to the user, and then invokes `callback`
with the user's response after it has been typed.

This will also resume the `input` stream used with `createInterface` if
it has been paused.

If `output` is set to `null` or `undefined` when calling `createInterface`,
nothing is displayed.

Example usage:

```js
rl.question('What is your favorite food?', (answer) => {
  console.log(`Oh, so your favorite food is ${answer}`);
});
```

### rl.resume()
<!-- YAML
added: v0.3.4
-->

Resumes the readline `input` stream.

### rl.setPrompt(prompt)
<!-- YAML
added: v0.1.98
-->

Sets the prompt, for example when you run `node` on the command line, you see
`> `, which is Node.js's prompt.

### rl.write(data[, key])
<!-- YAML
added: v0.1.98
-->

Writes `data` to `output` stream, unless `output` is set to `null` or
`undefined` when calling `createInterface`. `key` is an object literal to
represent a key sequence; available if the terminal is a TTY.

This will also resume the `input` stream if it has been paused.

Example:

```js
rl.write('Delete me!');
// Simulate ctrl+u to delete the line written previously
rl.write(null, {ctrl: true, name: 'u'});
```

## Events

### Event: 'close'
<!-- YAML
added: v0.1.98
-->

`function () {}`

Emitted when `close()` is called.

Also emitted when the `input` stream receives its `'end'` event. The `Interface`
instance should be considered "finished" once this is emitted. For example, when
the `input` stream receives `^D`, respectively known as `EOT`.

This event is also called if there is no `SIGINT` event listener present when
the `input` stream receives a `^C`, respectively known as `SIGINT`.

### Event: 'line'
<!-- YAML
added: v0.1.98
-->

`function (line) {}`

Emitted whenever the `input` stream receives an end of line (`\n`, `\r`, or
`\r\n`), usually received when the user hits enter, or return. This is a good
hook to listen for user input.

Example of listening for `'line'`:

```js
rl.on('line', (cmd) => {
  console.log(`You just typed: ${cmd}`);
});
```

### Event: 'pause'
<!-- YAML
added: v0.7.5
-->

`function () {}`

Emitted whenever the `input` stream is paused.

Also emitted whenever the `input` stream is not paused and receives the
`SIGCONT` event. (See events `SIGTSTP` and `SIGCONT`)

Example of listening for `'pause'`:

```js
rl.on('pause', () => {
  console.log('Readline paused.');
});
```

### Event: 'resume'
<!-- YAML
added: v0.7.5
-->

`function () {}`

Emitted whenever the `input` stream is resumed.

Example of listening for `'resume'`:

```js
rl.on('resume', () => {
  console.log('Readline resumed.');
});
```

### Event: 'SIGCONT'
<!-- YAML
added: v0.7.5
-->

`function () {}`

**This does not work on Windows.**

Emitted whenever the `input` stream is sent to the background with `^Z`,
respectively known as `SIGTSTP`, and then continued with `fg(1)`. This event
only emits if the stream was not paused before sending the program to the
background.

Example of listening for `SIGCONT`:

```js
rl.on('SIGCONT', () => {
  // `prompt` will automatically resume the stream
  rl.prompt();
});
```

### Event: 'SIGINT'
<!-- YAML
added: v0.3.0
-->

`function () {}`

Emitted whenever the `input` stream receives a `^C`, respectively known as
`SIGINT`. If there is no `SIGINT` event listener present when the `input`
stream receives a `SIGINT`, `pause` will be triggered.

Example of listening for `SIGINT`:

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

`function () {}`

**This does not work on Windows.**

Emitted whenever the `input` stream receives a `^Z`, respectively known as
`SIGTSTP`. If there is no `SIGTSTP` event listener present when the `input`
stream receives a `SIGTSTP`, the program will be sent to the background.

When the program is resumed with `fg`, the `'pause'` and `SIGCONT` events will be
emitted. You can use either to resume the stream.

The `'pause'` and `SIGCONT` events will not be triggered if the stream was paused
before the program was sent to the background.

Example of listening for `SIGTSTP`:

```js
rl.on('SIGTSTP', () => {
  // This will override SIGTSTP and prevent the program from going to the
  // background.
  console.log('Caught SIGTSTP.');
});
```

## Example: Tiny CLI

Here's an example of how to use all these together to craft a tiny command
line interface:

```js
const readline = require('readline');
const rl = readline.createInterface(process.stdin, process.stdout);

rl.setPrompt('OHAI> ');
rl.prompt();

rl.on('line', (line) => {
  switch(line.trim()) {
    case 'hello':
      console.log('world!');
      break;
    default:
      console.log('Say what? I might have heard `' + line.trim() + '`');
      break;
  }
  rl.prompt();
}).on('close', () => {
  console.log('Have a great day!');
  process.exit(0);
});
```

## Example: Read File Stream Line-by-Line

A common case for `readline`'s `input` option is to pass a filesystem readable
stream to it. This is how one could craft line-by-line parsing of a file:

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

## readline.clearLine(stream, dir)
<!-- YAML
added: v0.7.7
-->

Clears current line of given TTY stream in a specified direction.
`dir` should have one of following values:

* `-1` - to the left from cursor
* `1` - to the right from cursor
* `0` - the entire line

## readline.clearScreenDown(stream)
<!-- YAML
added: v0.7.7
-->

Clears the screen from the current position of the cursor down.

## readline.createInterface(options)
<!-- YAML
added: v0.1.98
-->

Creates a readline `Interface` instance. Accepts an `options` Object that takes
the following values:

 - `input` - the readable stream to listen to (Required).

 - `output` - the writable stream to write readline data to (Optional).

 - `completer` - an optional function that is used for Tab autocompletion. See
   below for an example of using this.

 - `terminal` - pass `true` if the `input` and `output` streams should be
   treated like a TTY, and have ANSI/VT100 escape codes written to it.
   Defaults to checking `isTTY` on the `output` stream upon instantiation.

 - `historySize` - maximum number of history lines retained. Defaults to `30`.

The `completer` function is given the current line entered by the user, and
is supposed to return an Array with 2 entries:

 1. An Array with matching entries for the completion.

 2. The substring that was used for the matching.

Which ends up looking something like:
`[[substr1, substr2, ...], originalsubstring]`.

Example:

```js
function completer(line) {
  var completions = '.help .error .exit .quit .q'.split(' ')
  var hits = completions.filter((c) => { return c.indexOf(line) == 0 })
  // show all completions if none found
  return [hits.length ? hits : completions, line]
}
```

Also `completer` can be run in async mode if it accepts two arguments:

```js
function completer(linePartial, callback) {
  callback(null, [['123'], linePartial]);
}
```

`createInterface` is commonly used with [`process.stdin`][] and
[`process.stdout`][] in order to accept user input:

```js
const readline = require('readline');
const rl = readline.createInterface({
  input: process.stdin,
  output: process.stdout
});
```

Once you have a readline instance, you most commonly listen for the
`'line'` event.

If `terminal` is `true` for this instance then the `output` stream will get
the best compatibility if it defines an `output.columns` property, and fires
a `'resize'` event on the `output` if/when the columns ever change
([`process.stdout`][] does this automatically when it is a TTY).

## readline.cursorTo(stream, x, y)
<!-- YAML
added: v0.7.7
-->

Move cursor to the specified position in a given TTY stream.

## readline.emitKeypressEvents(stream[, interface])
<!-- YAML
added: v0.7.7
-->

Causes `stream` to begin emitting `'keypress'` events corresponding to its
input.
Optionally, `interface` specifies a `readline.Interface` instance for which
autocompletion is disabled when copy-pasted input is detected.

## readline.moveCursor(stream, dx, dy)
<!-- YAML
added: v0.7.7
-->

Move cursor relative to it's current position in a given TTY stream.

[`process.stdin`]: process.html#process_process_stdin
[`process.stdout`]: process.html#process_process_stdout
