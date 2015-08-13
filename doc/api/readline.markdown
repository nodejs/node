# Readline

    Stability: 2 - Stable

To use this module, do `require('readline')`. Readline allows reading of a
stream (such as `process.stdin`) on a line-by-line basis.

Note that once you've invoked this module, your Node.js program will not
terminate until you've closed the interface. Here's how to allow your
program to gracefully exit:

    var readline = require('readline');

    var rl = readline.createInterface({
      input: process.stdin,
      output: process.stdout
    });

    rl.question("What do you think of Node.js? ", function(answer) {
      // TODO: Log the answer in a database
      console.log("Thank you for your valuable feedback:", answer);

      rl.close();
    });

## readline.createInterface(options)

Creates a readline `Interface` instance. Accepts an "options" Object that takes
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

    function completer(line) {
      var completions = '.help .error .exit .quit .q'.split(' ')
      var hits = completions.filter(function(c) { return c.indexOf(line) == 0 })
      // show all completions if none found
      return [hits.length ? hits : completions, line]
    }

Also `completer` can be run in async mode if it accepts two arguments:

    function completer(linePartial, callback) {
      callback(null, [['123'], linePartial]);
    }

`createInterface` is commonly used with `process.stdin` and
`process.stdout` in order to accept user input:

    var readline = require('readline');
    var rl = readline.createInterface({
      input: process.stdin,
      output: process.stdout
    });

Once you have a readline instance, you most commonly listen for the
`"line"` event.

If `terminal` is `true` for this instance then the `output` stream will get
the best compatibility if it defines an `output.columns` property, and fires
a `"resize"` event on the `output` if/when the columns ever change
(`process.stdout` does this automatically when it is a TTY).

## Class: Interface

The class that represents a readline interface with an input and output
stream.

### rl.setPrompt(prompt)

Sets the prompt, for example when you run `node` on the command line, you see
`> `, which is node.js's prompt.

### rl.prompt([preserveCursor])

Readies readline for input from the user, putting the current `setPrompt`
options on a new line, giving the user a new spot to write. Set `preserveCursor`
to `true` to prevent the cursor placement being reset to `0`.

This will also resume the `input` stream used with `createInterface` if it has
been paused.

If `output` is set to `null` or `undefined` when calling `createInterface`, the
prompt is not written.

### rl.question(query, callback)

Prepends the prompt with `query` and invokes `callback` with the user's
response. Displays the query to the user, and then invokes `callback`
with the user's response after it has been typed.

This will also resume the `input` stream used with `createInterface` if
it has been paused.

If `output` is set to `null` or `undefined` when calling `createInterface`,
nothing is displayed.

Example usage:

    interface.question('What is your favorite food?', function(answer) {
      console.log('Oh, so your favorite food is ' + answer);
    });

### rl.pause()

Pauses the readline `input` stream, allowing it to be resumed later if needed.

Note that this doesn't immediately pause the stream of events. Several events may be emitted after calling `pause`, including `line`.

### rl.resume()

Resumes the readline `input` stream.

### rl.close()

Closes the `Interface` instance, relinquishing control on the `input` and
`output` streams. The "close" event will also be emitted.

### rl.write(data[, key])

Writes `data` to `output` stream, unless `output` is set to `null` or
`undefined` when calling `createInterface`. `key` is an object literal to
represent a key sequence; available if the terminal is a TTY.

This will also resume the `input` stream if it has been paused.

Example:

    rl.write('Delete me!');
    // Simulate ctrl+u to delete the line written previously
    rl.write(null, {ctrl: true, name: 'u'});

## Events

### Event: 'line'

`function (line) {}`

Emitted whenever the `input` stream receives a `\n`, usually received when the
user hits enter, or return. This is a good hook to listen for user input.

Example of listening for `line`:

    rl.on('line', function (cmd) {
      console.log('You just typed: '+cmd);
    });

### Event: 'pause'

`function () {}`

Emitted whenever the `input` stream is paused.

Also emitted whenever the `input` stream is not paused and receives the
`SIGCONT` event. (See events `SIGTSTP` and `SIGCONT`)

Example of listening for `pause`:

    rl.on('pause', function() {
      console.log('Readline paused.');
    });

### Event: 'resume'

`function () {}`

Emitted whenever the `input` stream is resumed.

Example of listening for `resume`:

    rl.on('resume', function() {
      console.log('Readline resumed.');
    });

### Event: 'close'

`function () {}`

Emitted when `close()` is called.

Also emitted when the `input` stream receives its "end" event. The `Interface`
instance should be considered "finished" once this is emitted. For example, when
the `input` stream receives `^D`, respectively known as `EOT`.

This event is also called if there is no `SIGINT` event listener present when
the `input` stream receives a `^C`, respectively known as `SIGINT`.

### Event: 'SIGINT'

`function () {}`

Emitted whenever the `input` stream receives a `^C`, respectively known as
`SIGINT`. If there is no `SIGINT` event listener present when the `input`
stream receives a `SIGINT`, `pause` will be triggered.

Example of listening for `SIGINT`:

    rl.on('SIGINT', function() {
      rl.question('Are you sure you want to exit?', function(answer) {
        if (answer.match(/^y(es)?$/i)) rl.pause();
      });
    });

### Event: 'SIGTSTP'

`function () {}`

**This does not work on Windows.**

Emitted whenever the `input` stream receives a `^Z`, respectively known as
`SIGTSTP`. If there is no `SIGTSTP` event listener present when the `input`
stream receives a `SIGTSTP`, the program will be sent to the background.

When the program is resumed with `fg`, the `pause` and `SIGCONT` events will be
emitted. You can use either to resume the stream.

The `pause` and `SIGCONT` events will not be triggered if the stream was paused
before the program was sent to the background.

Example of listening for `SIGTSTP`:

    rl.on('SIGTSTP', function() {
      // This will override SIGTSTP and prevent the program from going to the
      // background.
      console.log('Caught SIGTSTP.');
    });

### Event: 'SIGCONT'

`function () {}`

**This does not work on Windows.**

Emitted whenever the `input` stream is sent to the background with `^Z`,
respectively known as `SIGTSTP`, and then continued with `fg(1)`. This event
only emits if the stream was not paused before sending the program to the
background.

Example of listening for `SIGCONT`:

    rl.on('SIGCONT', function() {
      // `prompt` will automatically resume the stream
      rl.prompt();
    });


## Example: Tiny CLI

Here's an example of how to use all these together to craft a tiny command
line interface:

    var readline = require('readline'),
        rl = readline.createInterface(process.stdin, process.stdout);

    rl.setPrompt('OHAI> ');
    rl.prompt();

    rl.on('line', function(line) {
      switch(line.trim()) {
        case 'hello':
          console.log('world!');
          break;
        default:
          console.log('Say what? I might have heard `' + line.trim() + '`');
          break;
      }
      rl.prompt();
    }).on('close', function() {
      console.log('Have a great day!');
      process.exit(0);
    });

## readline.cursorTo(stream, x, y)

Move cursor to the specified position in a given TTY stream.

## readline.moveCursor(stream, dx, dy)

Move cursor relative to it's current position in a given TTY stream.

## readline.clearLine(stream, dir)

Clears current line of given TTY stream in a specified direction.
`dir` should have one of following values:

* `-1` - to the left from cursor
* `1` - to the right from cursor
* `0` - the entire line

## readline.clearScreenDown(stream)

Clears the screen from the current position of the cursor down.
