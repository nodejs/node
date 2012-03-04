# TTY

    Stability: 3 - Stable

Use `require('tty')` to access this module.

Example:

    var tty = require('tty');
    process.stdin.resume();
    tty.setRawMode(true);
    process.stdin.on('keypress', function(char, key) {
      if (key && key.ctrl && key.name == 'c') {
        console.log('graceful exit');
        process.exit()
      }
    });



## tty.isatty(fd)

Returns `true` or `false` depending on if the `fd` is associated with a
terminal.


## tty.setRawMode(mode)

`mode` should be `true` or `false`. This sets the properties of the current
process's stdin fd to act either as a raw device or default.
