## util

These functions are in the module `'util'`. Use `require('util')` to access
them.


### util.debug(string)

A synchronous output function. Will block the process and
output `string` immediately to `stderr`.

    require('util').debug('message on stderr');


### util.log(string)

Output with timestamp on `stdout`.

    require('util').log('Timestmaped message.');


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


### util.pump(readableStream, writeableStream, [callback])

Experimental

Read the data from `readableStream` and send it to the `writableStream`.
When `writeableStream.write(data)` returns `false` `readableStream` will be
paused until the `drain` event occurs on the `writableStream`. `callback` gets
an error as its only argument and is called when `writableStream` is closed or
when an error occurs.
