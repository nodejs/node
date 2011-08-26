## console

Browser-like object for printing to stdout and stderr.

### console.log()

Prints to stdout with newline. This function can take multiple arguments in a
`printf()`-like way. Example:

    console.log('count: %d', count);

The first argument is a string that contains zero or more *placeholders*.
Each placeholder is replaced with the converted value from its corresponding
argument. Supported placeholders are:

* `%s` - String.
* `%d` - Number (both integer and float).
* `%j` - JSON.

If the placeholder does not have a corresponding argument, `undefined` is used.

    console.log('%s:%s', 'foo'); // 'foo:undefined'

If there are more arguments than placeholders, the extra arguments are
converted to strings with `util.inspect()` and these strings are concatenated,
delimited by a space.

    console.log('%s:%s', 'foo', 'bar', 'baz'); // 'foo:bar baz'

If the first argument is not a format string then `console.log()` prints
a string that is the concatenation of all its arguments separated by spaces.
Each argument is converted to a string with `util.inspect()`.

    console.log(1, 2, 3); // '1 2 3'


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
    for (var i = 0; i < 100; i++) {
      ;
    }
    console.timeEnd('100-elements');


### console.trace()

Print a stack trace to stderr of the current position.

### console.assert()

Same as `assert.ok()`.

