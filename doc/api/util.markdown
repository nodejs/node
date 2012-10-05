# util

    Stability: 5 - Locked

These functions are in the module `'util'`. Use `require('util')` to access
them.


## util.format(format, [...])

Returns a formatted string using the first argument as a `printf`-like format.

The first argument is a string that contains zero or more *placeholders*.
Each placeholder is replaced with the converted value from its corresponding
argument. Supported placeholders are:

* `%s` - String.
* `%d` - Number (both integer and float).
* `%j` - JSON.
* `%%` - single percent sign (`'%'`). This does not consume an argument.

If the placeholder does not have a corresponding argument, the placeholder is
not replaced.

    util.format('%s:%s', 'foo'); // 'foo:%s'

If there are more arguments than placeholders, the extra arguments are
converted to strings with `util.inspect()` and these strings are concatenated,
delimited by a space.

    util.format('%s:%s', 'foo', 'bar', 'baz'); // 'foo:bar baz'

If the first argument is not a format string then `util.format()` returns
a string that is the concatenation of all its arguments separated by spaces.
Each argument is converted to a string with `util.inspect()`.

    util.format(1, 2, 3); // '1 2 3'


## util.debug(string)

A synchronous output function. Will block the process and
output `string` immediately to `stderr`.

    require('util').debug('message on stderr');

## util.error([...])

Same as `util.debug()` except this will output all arguments immediately to
`stderr`.

## util.puts([...])

A synchronous output function. Will block the process and output all arguments
to `stdout` with newlines after each argument.

## util.print([...])

A synchronous output function. Will block the process, cast each argument to a
string then output to `stdout`. Does not place newlines after each argument.

## util.log(string)

Output with timestamp on `stdout`.

    require('util').log('Timestamped message.');


## util.inspect(object, [showHidden], [depth], [colors])

Return a string representation of `object`, which is useful for debugging.

If `showHidden` is `true`, then the object's non-enumerable properties will be
shown too. Defaults to `false`.

If `depth` is provided, it tells `inspect` how many times to recurse while
formatting the object. This is useful for inspecting large complicated objects.

The default is to only recurse twice.  To make it recurse indefinitely, pass
in `null` for `depth`.

If `colors` is `true`, the output will be styled with ANSI color codes.
Defaults to `false`.

Example of inspecting all properties of the `util` object:

    var util = require('util');

    console.log(util.inspect(util, true, null));

Objects also may define their own `inspect(depth)` function which `util.inspect()`
will invoke and use the result of when inspecting the object:

    var util = require('util');

    var obj = { name: 'nate' };
    obj.inspect = function(depth) {
      return '{' + this.name + '}';
    };

    util.inspect(obj);
      // "{nate}"


## util.isArray(object)

Returns `true` if the given "object" is an `Array`. `false` otherwise.

    var util = require('util');

    util.isArray([])
      // true
    util.isArray(new Array)
      // true
    util.isArray({})
      // false


## util.isRegExp(object)

Returns `true` if the given "object" is a `RegExp`. `false` otherwise.

    var util = require('util');

    util.isRegExp(/some regexp/)
      // true
    util.isRegExp(new RegExp('another regexp'))
      // true
    util.isRegExp({})
      // false


## util.isDate(object)

Returns `true` if the given "object" is a `Date`. `false` otherwise.

    var util = require('util');

    util.isDate(new Date())
      // true
    util.isDate(Date())
      // false (without 'new' returns a String)
    util.isDate({})
      // false


## util.isError(object)

Returns `true` if the given "object" is an `Error`. `false` otherwise.

    var util = require('util');

    util.isError(new Error())
      // true
    util.isError(new TypeError())
      // true
    util.isError({ name: 'Error', message: 'an error occurred' })
      // false


## util.pump(readableStream, writableStream, [callback])

    Stability: 0 - Deprecated: Use readableStream.pipe(writableStream)

Read the data from `readableStream` and send it to the `writableStream`.
When `writableStream.write(data)` returns `false` `readableStream` will be
paused until the `drain` event occurs on the `writableStream`. `callback` gets
an error as its only argument and is called when `writableStream` is closed or
when an error occurs.


## util.inherits(constructor, superConstructor)

Inherit the prototype methods from one
[constructor](https://developer.mozilla.org/en/JavaScript/Reference/Global_Objects/Object/constructor)
into another.  The prototype of `constructor` will be set to a new
object created from `superConstructor`.

As an additional convenience, `superConstructor` will be accessible
through the `constructor.super_` property.

    var util = require("util");
    var events = require("events");

    function MyStream() {
        events.EventEmitter.call(this);
    }

    util.inherits(MyStream, events.EventEmitter);

    MyStream.prototype.write = function(data) {
        this.emit("data", data);
    }

    var stream = new MyStream();

    console.log(stream instanceof events.EventEmitter); // true
    console.log(MyStream.super_ === events.EventEmitter); // true

    stream.on("data", function(data) {
        console.log('Received data: "' + data + '"');
    })
    stream.write("It works!"); // Received data: "It works!"
