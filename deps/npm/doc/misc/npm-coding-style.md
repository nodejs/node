npm-coding-style(7) -- npm's "funny" coding style
=================================================

## DESCRIPTION

npm's coding style is a bit unconventional.  It is not different for
difference's sake, but rather a carefully crafted style that is
designed to reduce visual clutter and make bugs more apparent.

If you want to contribute to npm (which is very encouraged), you should
make your code conform to npm's style.

Note: this concerns npm's code not the specific packages that you can download from the npm registry.

## Line Length

Keep lines shorter than 80 characters.  It's better for lines to be
too short than to be too long.  Break up long lists, objects, and other
statements onto multiple lines.

## Indentation

Two-spaces.  Tabs are better, but they look like hell in web browsers
(and on GitHub), and node uses 2 spaces, so that's that.

Configure your editor appropriately.

## Curly braces

Curly braces belong on the same line as the thing that necessitates them.

Bad:

    function ()
    {

Good:

    function () {

If a block needs to wrap to the next line, use a curly brace.  Don't
use it if it doesn't.

Bad:

    if (foo) { bar() }
    while (foo)
      bar()

Good:

    if (foo) bar()
    while (foo) {
      bar()
    }

## Semicolons

Don't use them except in four situations:

* `for (;;)` loops.  They're actually required.
* null loops like: `while (something) ;` (But you'd better have a good
  reason for doing that.)
* `case 'foo': doSomething(); break`
* In front of a leading `(` or `[` at the start of the line.
  This prevents the expression from being interpreted
  as a function call or property access, respectively.

Some examples of good semicolon usage:

    ;(x || y).doSomething()
    ;[a, b, c].forEach(doSomething)
    for (var i = 0; i < 10; i ++) {
      switch (state) {
        case 'begin': start(); continue
        case 'end': finish(); break
        default: throw new Error('unknown state')
      }
      end()
    }

Note that starting lines with `-` and `+` also should be prefixed
with a semicolon, but this is much less common.

## Comma First

If there is a list of things separated by commas, and it wraps
across multiple lines, put the comma at the start of the next
line, directly below the token that starts the list.  Put the
final token in the list on a line by itself.  For example:

    var magicWords = [ 'abracadabra'
                     , 'gesundheit'
                     , 'ventrilo'
                     ]
      , spells = { 'fireball' : function () { setOnFire() }
                 , 'water' : function () { putOut() }
                 }
      , a = 1
      , b = 'abc'
      , etc
      , somethingElse

## Quotes
Use single quotes for strings except to avoid escaping.

Bad:

    var notOk = "Just double quotes"

Good:

    var ok = 'String contains "double" quotes'
    var alsoOk = "String contains 'single' quotes or apostrophe"

## Whitespace

Put a single space in front of `(` for anything other than a function call.
Also use a single space wherever it makes things more readable.

Don't leave trailing whitespace at the end of lines.  Don't indent empty
lines.  Don't use more spaces than are helpful.

## Functions

Use named functions.  They make stack traces a lot easier to read.

## Callbacks, Sync/async Style

Use the asynchronous/non-blocking versions of things as much as possible.
It might make more sense for npm to use the synchronous fs APIs, but this
way, the fs and http and child process stuff all uses the same callback-passing
methodology.

The callback should always be the last argument in the list.  Its first
argument is the Error or null.

Be very careful never to ever ever throw anything.  It's worse than useless.
Just send the error message back as the first argument to the callback.

## Errors

Always create a new Error object with your message.  Don't just return a
string message to the callback.  Stack traces are handy.

## Logging

Logging is done using the [npmlog](https://github.com/npm/npmlog)
utility.

Please clean up logs when they are no longer helpful.  In particular,
logging the same object over and over again is not helpful.  Logs should
report what's happening so that it's easier to track down where a fault
occurs.

Use appropriate log levels.  See `npm-config(7)` and search for
"loglevel".

## Case, naming, etc.

Use `lowerCamelCase` for multiword identifiers when they refer to objects,
functions, methods, properties, or anything not specified in this section.

Use `UpperCamelCase` for class names (things that you'd pass to "new").

Use `all-lower-hyphen-css-case` for multiword filenames and config keys.

Use named functions.  They make stack traces easier to follow.

Use `CAPS_SNAKE_CASE` for constants, things that should never change
and are rarely used.

Use a single uppercase letter for function names where the function
would normally be anonymous, but needs to call itself recursively.  It
makes it clear that it's a "throwaway" function.

## null, undefined, false, 0

Boolean variables and functions should always be either `true` or
`false`.  Don't set it to 0 unless it's supposed to be a number.

When something is intentionally missing or removed, set it to `null`.

Don't set things to `undefined`.  Reserve that value to mean "not yet
set to anything."

Boolean objects are forbidden.

## SEE ALSO

* npm-developers(7)
* npm(1)
