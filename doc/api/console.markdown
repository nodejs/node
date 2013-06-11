# console

    Stability: 4 - API Frozen

* {Object}

<!--type=global-->

For printing to stdout and stderr.  Similar to the console object functions
provided by most web browsers, here the output is sent to stdout or stderr.

The console functions are synchronous when the destination is a terminal or
a file (to avoid lost messages in case of premature exit) and asynchronous
when it's a pipe (to avoid blocking for long periods of time).

That is, in the following example, stdout is non-blocking while stderr
is blocking:

    $ node script.js 2> error.log | tee info.log

In daily use, the blocking/non-blocking dichotomy is not something you
should worry about unless you log huge amounts of data.


## console.log([data], [...])

Prints to stdout with newline. This function can take multiple arguments in a
`printf()`-like way. Example:

    console.log('count: %d', count);

If formatting elements are not found in the first string then `util.inspect`
is used on each argument.  See [util.format()][] for more information.

## console.info([data], [...])

Same as `console.log`.

## console.error([data], [...])

Same as `console.log` but prints to stderr.

## console.warn([data], [...])

Same as `console.error`.

## console.dir(obj)

Uses `util.inspect` on `obj` and prints resulting string to stdout.

## console.time(label)

Mark a time.

## console.timeEnd(label)

Finish timer, record output. Example:

    console.time('100-elements');
    for (var i = 0; i < 100; i++) {
      ;
    }
    console.timeEnd('100-elements');

## console.trace(label)

Print a stack trace to stderr of the current position.

## console.assert(expression, [message])

Same as [assert.ok()][] where if the `expression` evaluates as `false` throw an
AssertionError with `message`.

[assert.ok()]: assert.html#assert_assert_value_message_assert_ok_value_message
[util.format()]: util.html#util_util_format_format
