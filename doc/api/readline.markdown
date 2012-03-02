# Readline

    Stability: 3 - Stable

To use this module, do `require('readline')`. Readline allows reading of a
stream (such as STDIN) on a line-by-line basis.

Note that once you've invoked this module, your node program will not
terminate until you've closed the interface, and the STDIN stream. Here's how
to allow your program to gracefully terminate:

    var rl = require('readline');

    var i = rl.createInterface(process.stdin, process.stdout, null);
    i.question("What do you think of node.js?", function(answer) {
      // TODO: Log the answer in a database
      console.log("Thank you for your valuable feedback.");

      // These two lines together allow the program to terminate. Without
      // them, it would run forever.
      i.close();
      process.stdin.destroy();
    });

## rl.createInterface(input, output, completer)

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

## Class: Interface

The class that represents a readline interface with a stdin and stdout
stream.

### rl.setPrompt(prompt, length)

Sets the prompt, for example when you run `node` on the command line, you see
`> `, which is node's prompt.

### rl.prompt()

Readies readline for input from the user, putting the current `setPrompt`
options on a new line, giving the user a new spot to write.

### rl.question(query, callback)

Prepends the prompt with `query` and invokes `callback` with the user's
response. Displays the query to the user, and then invokes `callback` with the
user's response after it has been typed.

Example usage:

    interface.question('What is your favorite food?', function(answer) {
      console.log('Oh, so your favorite food is ' + answer);
    });

### rl.close()

  Closes tty.

### rl.pause()

  Pauses tty.

### rl.resume()

  Resumes tty.

### rl.write()

  Writes to tty.

### Event: 'line'

`function (line) {}`

Emitted whenever the `in` stream receives a `\n`, usually received when the
user hits enter, or return. This is a good hook to listen for user input.

Example of listening for `line`:

    rl.on('line', function (cmd) {
      console.log('You just typed: '+cmd);
    });

### Event: 'close'

`function () {}`

Emitted whenever the `in` stream receives a `^C` or `^D`, respectively known
as `SIGINT` and `EOT`. This is a good way to know the user is finished using
your program.

Example of listening for `close`, and exiting the program afterward:

    rl.on('close', function() {
      console.log('goodbye!');
      process.exit(0);
    });

Here's an example of how to use all these together to craft a tiny command
line interface:

    var readline = require('readline'),
      rl = readline.createInterface(process.stdin, process.stdout),
      prefix = 'OHAI> ';

    rl.on('line', function(line) {
      switch(line.trim()) {
        case 'hello':
          console.log('world!');
          break;
        default:
          console.log('Say what? I might have heard `' + line.trim() + '`');
          break;
      }
      rl.setPrompt(prefix, prefix.length);
      rl.prompt();
    }).on('close', function() {
      console.log('Have a great day!');
      process.exit(0);
    });
    console.log(prefix + 'Good to see you. Try typing stuff.');
    rl.setPrompt(prefix, prefix.length);
    rl.prompt();


Take a look at this slightly more complicated
[example](https://gist.github.com/901104), and
[http-console](https://github.com/cloudhead/http-console) for a real-life use
case.
