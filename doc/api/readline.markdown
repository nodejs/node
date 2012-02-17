## Readline

To use this module, do `require('readline')`. Readline allows reading of a
stream (such as STDIN) on a line-by-line basis.

Note that once you've invoked this module, your node program will not
terminate until you've paused the interface. Here's how to allow your
program to gracefully pause:

    var rl = require('readline');

    var i = rl.createInterface(process.stdin, process.stdout, null);
    i.question("What do you think of node.js?", function(answer) {
      // TODO: Log the answer in a database
      console.log("Thank you for your valuable feedback.");

      i.pause();
    });

### rl.createInterface(input, output, completer)

Takes two streams and creates a readline interface. The `completer` function
is used for autocompletion. When given a substring, it returns `[[substr1,
substr2, ...], originalsubstring]`.

Also `completer` can be run in async mode if it accepts two arguments:

  function completer(linePartial, callback) {
    callback(null, [['123'], linePartial]);
  }

`createInterface` is commonly used with `process.stdin` and
`process.stdout` in order to accept user input:

    var readline = require('readline'),
      rl = readline.createInterface(process.stdin, process.stdout);

### rl.setPrompt(prompt, length)

Sets the prompt, for example when you run `node` on the command line, you see
`> `, which is node's prompt.

### rl.prompt()

Readies readline for input from the user, putting the current `setPrompt`
options on a new line, giving the user a new spot to write.

This will also resume the `in` stream used with `createInterface` if it has
been paused.

<!-- ### rl.getColumns() Not available? -->

### rl.question(query, callback)

Prepends the prompt with `query` and invokes `callback` with the user's
response. Displays the query to the user, and then invokes `callback` with the
user's response after it has been typed.

This will also resume the `in` stream used with `createInterface` if it has
been paused.

Example usage:

    interface.question('What is your favorite food?', function(answer) {
      console.log('Oh, so your favorite food is ' + answer);
    });

### rl.pause()

Pauses the readline `in` stream, allowing it to be resumed later if needed.

### rl.resume()

Resumes the readline `in` stream.

### rl.write()

Writes to tty.

This will also resume the `in` stream used with `createInterface` if it has
been paused.

### Event: 'line'

`function (line) {}`

Emitted whenever the `in` stream receives a `\n`, usually received when the
user hits enter, or return. This is a good hook to listen for user input.

Example of listening for `line`:

    rl.on('line', function (cmd) {
      console.log('You just typed: '+cmd);
    });

### Event: 'pause'

`function () {}`

Emitted whenever the `in` stream is paused or receives `^D`, respectively known
as `EOT`. This event is also called if there is no `SIGINT` event listener
present when the `in` stream receives a `^C`, respectively known as `SIGINT`.

Also emitted whenever the `in` stream is not paused and receives the `SIGCONT`
event. (See events `SIGTSTP` and `SIGCONT`)

Example of listening for `pause`:

    rl.on('pause', function() {
      console.log('Readline paused.');
    });

### Event: 'resume'

`function () {}`

Emitted whenever the `in` stream is resumed.

Example of listening for `resume`:

    rl.on('resume', function() {
      console.log('Readline resumed.');
    });

### Event: 'SIGINT'

`function () {}`

Emitted whenever the `in` stream receives a `^C`, respectively known as
`SIGINT`. If there is no `SIGINT` event listener present when the `in` stream
receives a `SIGINT`, `pause` will be triggered.

Example of listening for `SIGINT`:

    rl.on('SIGINT', function() {
      rl.question('Are you sure you want to exit?', function(answer) {
        if (answer.match(/^y(es)?$/i)) rl.pause();
      });
    });

### Event: 'SIGTSTP'

`function () {}`

**This does not work on Windows.**

Emitted whenever the `in` stream receives a `^Z`, respectively known as
`SIGTSTP`. If there is no `SIGTSTP` event listener present when the `in` stream
receives a `SIGTSTP`, the program will be sent to the background.

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

Emitted whenever the `in` stream is sent to the background with `^Z`,
respectively known as `SIGTSTP`, and then continued with `fg`. This event only
emits if the stream was not paused before sending the program to the
background.

Example of listening for `SIGCONT`:

    rl.on('SIGCONT', function() {
      // `prompt` will automatically resume the stream
      rl.prompt();
    });


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
    }).on('pause', function() {
      console.log('Have a great day!');
      process.exit(0);
    });

