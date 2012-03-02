# console

    Stability: 4 - API Frozen

* {Object}

<!--type=global-->

For printing to stdout and stderr.  Similar to the console object functions
provided by most web browsers, here the output is sent to stdout or stderr.


## console.log()

Prints to stdout with newline. This function can take multiple arguments in a
`printf()`-like way. Example:

    console.log('count: %d', count);

If formatting elements are not found in the first string then `util.inspect`
is used on each argument.
See [util.format()](util.html#util.format) for more information.

## console.info()

Same as `console.log`.

## console.warn()
## console.error()

Same as `console.log` but prints to stderr.

## console.dir(obj)

Uses `util.inspect` on `obj` and prints resulting string to stderr.

## console.time(label)

Mark a time.


## console.timeEnd(label)

Finish timer, record output. Example

    console.time('100-elements');
    for (var i = 0; i < 100; i++) {
      ;
    }
    console.timeEnd('100-elements');


## console.trace()

Print a stack trace to stderr of the current position.

## console.assert()

Same as `assert.ok()`.

