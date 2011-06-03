## util

These functions are in the module `'util'`. Use `require('util')` to access
them.


### util.debug(string)

A synchronous output function. Will block the process and
output `string` immediately to `stderr`.

    require('util').debug('message on stderr');


### util.log(string)

Output with timestamp on `stdout`.

    require('util').log('Timestamped message.');


### util.inspect(object, showHidden=false, depth=2)

Return a string representation of `object`, which is useful for debugging.

If `showHidden` is `true`, then the object's non-enumerable properties will be
shown too.

If `depth` is provided, it tells `inspect` how many times to recurse while
formatting the object. This is useful for inspecting large complicated objects.

The default is to only recurse twice.  To make it recurse indefinitely, pass
in `null` for `depth`.

Example of inspecting all properties of the `util` object:

    var util = require('util');

    console.log(util.inspect(util, true, null));


### util.pump(readableStream, writableStream, [callback])

Experimental

Read the data from `readableStream` and send it to the `writableStream`.
When `writableStream.write(data)` returns `false` `readableStream` will be
paused until the `drain` event occurs on the `writableStream`. `callback` gets
an error as its only argument and is called when `writableStream` is closed or
when an error occurs.


### util.inherits(constructor, superConstructor)

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
