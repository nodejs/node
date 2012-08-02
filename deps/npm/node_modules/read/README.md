## read

For reading user input from stdin.

Similar to the `readline` builtin's `question()` method, but with a
few more features.

## USAGE

```javascript
var read = require("read")
read(options, callback)
```

The callback gets called with either the user input, or the default
specified, or an error, as `callback(error, result, isDefault)`
node style.

## OPTIONS

Every option is optional.

* `prompt` What to write to stdout before reading input.
* `silent` Don't echo the output as the user types it.
* `replace` Replace silenced characters with the supplied character value.
* `timeout` Number of ms to wait for user input before giving up.
* `default` The default value if the user enters nothing.
* `edit` Allow the user to edit the default value.
* `terminal` Treat the output as a TTY, whether it is or not.
* `stdin` Readable stream to get input data from. (default `process.stdin`)
* `stdout` Writeable stream to write prompts to. (default: `process.stdout`)

If silent is true, and the input is a TTY, then read will set raw
mode, and read character by character.

## CONTRIBUTING

Patches welcome.
