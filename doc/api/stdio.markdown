## console

Browser-like object for printing to stdout and stderr.

### console.log()

Prints to stdout with newline. This function can take multiple arguments in a
`printf()`-like way. Example:

    console.log('count: %d', count);

If formating elements are not found in the first string then `util.inspect`
is used on each argument.

### console.info()

Same as `console.log`.

### console.warn()
### console.error()

Same as `console.log` but prints to stderr.

### console.dir(obj)

Uses `util.inspect` on `obj` and prints resulting string to stderr.

### console.time(label)

Mark a time.


### console.timeEnd(label)

Finish timer, record output. Example

    console.time('100-elements');
    while (var i = 0; i < 100; i++) {
      ;
    }
    console.timeEnd('100-elements');


### console.trace()

Print a stack trace to stderr of the current position.

### console.assert()

Same as `assert.ok()`.

