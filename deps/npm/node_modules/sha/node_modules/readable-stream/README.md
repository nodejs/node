# readable-stream

A new class of streams for Node.js

This module provides the new Stream base classes introduced in Node
v0.10, for use in Node v0.8.  You can use it to have programs that
have to work with node v0.8, while being forward-compatible for v0.10
and beyond.  When you drop support for v0.8, you can remove this
module, and only use the native streams.

This is almost exactly the same codebase as appears in Node v0.10.
However:

1. The exported object is actually the Readable class.  Decorating the
   native `stream` module would be global pollution.
2. In v0.10, you can safely use `base64` as an argument to
   `setEncoding` in Readable streams.  However, in v0.8, the
   StringDecoder class has no `end()` method, which is problematic for
   Base64.  So, don't use that, because it'll break and be weird.

Other than that, the API is the same as `require('stream')` in v0.10,
so the API docs are reproduced below.

----------

    Stability: 2 - Unstable

A stream is an abstract interface implemented by various objects in
Node.  For example a request to an HTTP server is a stream, as is
stdout. Streams are readable, writable, or both. All streams are
instances of [EventEmitter][]

You can load the Stream base classes by doing `require('stream')`.
There are base classes provided for Readable streams, Writable
streams, Duplex streams, and Transform streams.

## Compatibility

In earlier versions of Node, the Readable stream interface was
simpler, but also less powerful and less useful.

* Rather than waiting for you to call the `read()` method, `'data'`
  events would start emitting immediately.  If you needed to do some
  I/O to decide how to handle data, then you had to store the chunks
  in some kind of buffer so that they would not be lost.
* The `pause()` method was advisory, rather than guaranteed.  This
  meant that you still had to be prepared to receive `'data'` events
  even when the stream was in a paused state.

In Node v0.10, the Readable class described below was added.  For
backwards compatibility with older Node programs, Readable streams
switch into "old mode" when a `'data'` event handler is added, or when
the `pause()` or `resume()` methods are called.  The effect is that,
even if you are not using the new `read()` method and `'readable'`
event, you no longer have to worry about losing `'data'` chunks.

Most programs will continue to function normally.  However, this
introduces an edge case in the following conditions:

* No `'data'` event handler is added.
* The `pause()` and `resume()` methods are never called.

For example, consider the following code:

```javascript
// WARNING!  BROKEN!
net.createServer(function(socket) {

  // we add an 'end' method, but never consume the data
  socket.on('end', function() {
    // It will never get here.
    socket.end('I got your message (but didnt read it)\n');
  });

}).listen(1337);
```

In versions of node prior to v0.10, the incoming message data would be
simply discarded.  However, in Node v0.10 and beyond, the socket will
remain paused forever.

The workaround in this situation is to call the `resume()` method to
trigger "old mode" behavior:

```javascript
// Workaround
net.createServer(function(socket) {

  socket.on('end', function() {
    socket.end('I got your message (but didnt read it)\n');
  });

  // start the flow of data, discarding it.
  socket.resume();

}).listen(1337);
```

In addition to new Readable streams switching into old-mode, pre-v0.10
style streams can be wrapped in a Readable class using the `wrap()`
method.

## Class: stream.Readable

<!--type=class-->

A `Readable Stream` has the following methods, members, and events.

Note that `stream.Readable` is an abstract class designed to be
extended with an underlying implementation of the `_read(size)`
method. (See below.)

### new stream.Readable([options])

* `options` {Object}
  * `highWaterMark` {Number} The maximum number of bytes to store in
    the internal buffer before ceasing to read from the underlying
    resource.  Default=16kb
  * `encoding` {String} If specified, then buffers will be decoded to
    strings using the specified encoding.  Default=null
  * `objectMode` {Boolean} Whether this stream should behave
    as a stream of objects. Meaning that stream.read(n) returns
    a single value instead of a Buffer of size n

In classes that extend the Readable class, make sure to call the
constructor so that the buffering settings can be properly
initialized.

### readable.\_read(size)

* `size` {Number} Number of bytes to read asynchronously

Note: **This function should NOT be called directly.**  It should be
implemented by child classes, and called by the internal Readable
class methods only.

All Readable stream implementations must provide a `_read` method
to fetch data from the underlying resource.

This method is prefixed with an underscore because it is internal to
the class that defines it, and should not be called directly by user
programs.  However, you **are** expected to override this method in
your own extension classes.

When data is available, put it into the read queue by calling
`readable.push(chunk)`.  If `push` returns false, then you should stop
reading.  When `_read` is called again, you should start pushing more
data.

The `size` argument is advisory.  Implementations where a "read" is a
single call that returns data can use this to know how much data to
fetch.  Implementations where that is not relevant, such as TCP or
TLS, may ignore this argument, and simply provide data whenever it
becomes available.  There is no need, for example to "wait" until
`size` bytes are available before calling `stream.push(chunk)`.

### readable.push(chunk)

* `chunk` {Buffer | null | String} Chunk of data to push into the read queue
* return {Boolean} Whether or not more pushes should be performed

Note: **This function should be called by Readable implementors, NOT
by consumers of Readable subclasses.**  The `_read()` function will not
be called again until at least one `push(chunk)` call is made.  If no
data is available, then you MAY call `push('')` (an empty string) to
allow a future `_read` call, without adding any data to the queue.

The `Readable` class works by putting data into a read queue to be
pulled out later by calling the `read()` method when the `'readable'`
event fires.

The `push()` method will explicitly insert some data into the read
queue.  If it is called with `null` then it will signal the end of the
data.

In some cases, you may be wrapping a lower-level source which has some
sort of pause/resume mechanism, and a data callback.  In those cases,
you could wrap the low-level source object by doing something like
this:

```javascript
// source is an object with readStop() and readStart() methods,
// and an `ondata` member that gets called when it has data, and
// an `onend` member that gets called when the data is over.

var stream = new Readable();

source.ondata = function(chunk) {
  // if push() returns false, then we need to stop reading from source
  if (!stream.push(chunk))
    source.readStop();
};

source.onend = function() {
  stream.push(null);
};

// _read will be called when the stream wants to pull more data in
// the advisory size argument is ignored in this case.
stream._read = function(n) {
  source.readStart();
};
```

### readable.unshift(chunk)

* `chunk` {Buffer | null | String} Chunk of data to unshift onto the read queue
* return {Boolean} Whether or not more pushes should be performed

This is the corollary of `readable.push(chunk)`.  Rather than putting
the data at the *end* of the read queue, it puts it at the *front* of
the read queue.

This is useful in certain use-cases where a stream is being consumed
by a parser, which needs to "un-consume" some data that it has
optimistically pulled out of the source.

```javascript
// A parser for a simple data protocol.
// The "header" is a JSON object, followed by 2 \n characters, and
// then a message body.
//
// Note: This can be done more simply as a Transform stream.  See below.

function SimpleProtocol(source, options) {
  if (!(this instanceof SimpleProtocol))
    return new SimpleProtocol(options);

  Readable.call(this, options);
  this._inBody = false;
  this._sawFirstCr = false;

  // source is a readable stream, such as a socket or file
  this._source = source;

  var self = this;
  source.on('end', function() {
    self.push(null);
  });

  // give it a kick whenever the source is readable
  // read(0) will not consume any bytes
  source.on('readable', function() {
    self.read(0);
  });

  this._rawHeader = [];
  this.header = null;
}

SimpleProtocol.prototype = Object.create(
  Readable.prototype, { constructor: { value: SimpleProtocol }});

SimpleProtocol.prototype._read = function(n) {
  if (!this._inBody) {
    var chunk = this._source.read();

    // if the source doesn't have data, we don't have data yet.
    if (chunk === null)
      return this.push('');

    // check if the chunk has a \n\n
    var split = -1;
    for (var i = 0; i < chunk.length; i++) {
      if (chunk[i] === 10) { // '\n'
        if (this._sawFirstCr) {
          split = i;
          break;
        } else {
          this._sawFirstCr = true;
        }
      } else {
        this._sawFirstCr = false;
      }
    }

    if (split === -1) {
      // still waiting for the \n\n
      // stash the chunk, and try again.
      this._rawHeader.push(chunk);
      this.push('');
    } else {
      this._inBody = true;
      var h = chunk.slice(0, split);
      this._rawHeader.push(h);
      var header = Buffer.concat(this._rawHeader).toString();
      try {
        this.header = JSON.parse(header);
      } catch (er) {
        this.emit('error', new Error('invalid simple protocol data'));
        return;
      }
      // now, because we got some extra data, unshift the rest
      // back into the read queue so that our consumer will see it.
      var b = chunk.slice(split);
      this.unshift(b);

      // and let them know that we are done parsing the header.
      this.emit('header', this.header);
    }
  } else {
    // from there on, just provide the data to our consumer.
    // careful not to push(null), since that would indicate EOF.
    var chunk = this._source.read();
    if (chunk) this.push(chunk);
  }
};

// Usage:
var parser = new SimpleProtocol(source);
// Now parser is a readable stream that will emit 'header'
// with the parsed header data.
```

### readable.wrap(stream)

* `stream` {Stream} An "old style" readable stream

If you are using an older Node library that emits `'data'` events and
has a `pause()` method that is advisory only, then you can use the
`wrap()` method to create a Readable stream that uses the old stream
as its data source.

For example:

```javascript
var OldReader = require('./old-api-module.js').OldReader;
var oreader = new OldReader;
var Readable = require('stream').Readable;
var myReader = new Readable().wrap(oreader);

myReader.on('readable', function() {
  myReader.read(); // etc.
});
```

### Event: 'readable'

When there is data ready to be consumed, this event will fire.

When this event emits, call the `read()` method to consume the data.

### Event: 'end'

Emitted when the stream has received an EOF (FIN in TCP terminology).
Indicates that no more `'data'` events will happen. If the stream is
also writable, it may be possible to continue writing.

### Event: 'data'

The `'data'` event emits either a `Buffer` (by default) or a string if
`setEncoding()` was used.

Note that adding a `'data'` event listener will switch the Readable
stream into "old mode", where data is emitted as soon as it is
available, rather than waiting for you to call `read()` to consume it.

### Event: 'error'

Emitted if there was an error receiving data.

### Event: 'close'

Emitted when the underlying resource (for example, the backing file
descriptor) has been closed. Not all streams will emit this.

### readable.setEncoding(encoding)

Makes the `'data'` event emit a string instead of a `Buffer`. `encoding`
can be `'utf8'`, `'utf16le'` (`'ucs2'`), `'ascii'`, or `'hex'`.

The encoding can also be set by specifying an `encoding` field to the
constructor.

### readable.read([size])

* `size` {Number | null} Optional number of bytes to read.
* Return: {Buffer | String | null}

Note: **This function SHOULD be called by Readable stream users.**

Call this method to consume data once the `'readable'` event is
emitted.

The `size` argument will set a minimum number of bytes that you are
interested in.  If not set, then the entire content of the internal
buffer is returned.

If there is no data to consume, or if there are fewer bytes in the
internal buffer than the `size` argument, then `null` is returned, and
a future `'readable'` event will be emitted when more is available.

Calling `stream.read(0)` will always return `null`, and will trigger a
refresh of the internal buffer, but otherwise be a no-op.

### readable.pipe(destination, [options])

* `destination` {Writable Stream}
* `options` {Object} Optional
  * `end` {Boolean} Default=true

Connects this readable stream to `destination` WriteStream. Incoming
data on this stream gets written to `destination`.  Properly manages
back-pressure so that a slow destination will not be overwhelmed by a
fast readable stream.

This function returns the `destination` stream.

For example, emulating the Unix `cat` command:

    process.stdin.pipe(process.stdout);

By default `end()` is called on the destination when the source stream
emits `end`, so that `destination` is no longer writable. Pass `{ end:
false }` as `options` to keep the destination stream open.

This keeps `writer` open so that "Goodbye" can be written at the
end.

    reader.pipe(writer, { end: false });
    reader.on("end", function() {
      writer.end("Goodbye\n");
    });

Note that `process.stderr` and `process.stdout` are never closed until
the process exits, regardless of the specified options.

### readable.unpipe([destination])

* `destination` {Writable Stream} Optional

Undo a previously established `pipe()`.  If no destination is
provided, then all previously established pipes are removed.

### readable.pause()

Switches the readable stream into "old mode", where data is emitted
using a `'data'` event rather than being buffered for consumption via
the `read()` method.

Ceases the flow of data.  No `'data'` events are emitted while the
stream is in a paused state.

### readable.resume()

Switches the readable stream into "old mode", where data is emitted
using a `'data'` event rather than being buffered for consumption via
the `read()` method.

Resumes the incoming `'data'` events after a `pause()`.


## Class: stream.Writable

<!--type=class-->

A `Writable` Stream has the following methods, members, and events.

Note that `stream.Writable` is an abstract class designed to be
extended with an underlying implementation of the
`_write(chunk, encoding, cb)` method. (See below.)

### new stream.Writable([options])

* `options` {Object}
  * `highWaterMark` {Number} Buffer level when `write()` starts
    returning false. Default=16kb
  * `decodeStrings` {Boolean} Whether or not to decode strings into
    Buffers before passing them to `_write()`.  Default=true

In classes that extend the Writable class, make sure to call the
constructor so that the buffering settings can be properly
initialized.

### writable.\_write(chunk, encoding, callback)

* `chunk` {Buffer | String} The chunk to be written.  Will always
  be a buffer unless the `decodeStrings` option was set to `false`.
* `encoding` {String} If the chunk is a string, then this is the
  encoding type.  Ignore chunk is a buffer.  Note that chunk will
  **always** be a buffer unless the `decodeStrings` option is
  explicitly set to `false`.
* `callback` {Function} Call this function (optionally with an error
  argument) when you are done processing the supplied chunk.

All Writable stream implementations must provide a `_write` method to
send data to the underlying resource.

Note: **This function MUST NOT be called directly.**  It should be
implemented by child classes, and called by the internal Writable
class methods only.

Call the callback using the standard `callback(error)` pattern to
signal that the write completed successfully or with an error.

If the `decodeStrings` flag is set in the constructor options, then
`chunk` may be a string rather than a Buffer, and `encoding` will
indicate the sort of string that it is.  This is to support
implementations that have an optimized handling for certain string
data encodings.  If you do not explicitly set the `decodeStrings`
option to `false`, then you can safely ignore the `encoding` argument,
and assume that `chunk` will always be a Buffer.

This method is prefixed with an underscore because it is internal to
the class that defines it, and should not be called directly by user
programs.  However, you **are** expected to override this method in
your own extension classes.


### writable.write(chunk, [encoding], [callback])

* `chunk` {Buffer | String} Data to be written
* `encoding` {String} Optional.  If `chunk` is a string, then encoding
  defaults to `'utf8'`
* `callback` {Function} Optional.  Called when this chunk is
  successfully written.
* Returns {Boolean}

Writes `chunk` to the stream.  Returns `true` if the data has been
flushed to the underlying resource.  Returns `false` to indicate that
the buffer is full, and the data will be sent out in the future. The
`'drain'` event will indicate when the buffer is empty again.

The specifics of when `write()` will return false, is determined by
the `highWaterMark` option provided to the constructor.

### writable.end([chunk], [encoding], [callback])

* `chunk` {Buffer | String} Optional final data to be written
* `encoding` {String} Optional.  If `chunk` is a string, then encoding
  defaults to `'utf8'`
* `callback` {Function} Optional.  Called when the final chunk is
  successfully written.

Call this method to signal the end of the data being written to the
stream.

### Event: 'drain'

Emitted when the stream's write queue empties and it's safe to write
without buffering again. Listen for it when `stream.write()` returns
`false`.

### Event: 'close'

Emitted when the underlying resource (for example, the backing file
descriptor) has been closed. Not all streams will emit this.

### Event: 'finish'

When `end()` is called and there are no more chunks to write, this
event is emitted.

### Event: 'pipe'

* `source` {Readable Stream}

Emitted when the stream is passed to a readable stream's pipe method.

### Event 'unpipe'

* `source` {Readable Stream}

Emitted when a previously established `pipe()` is removed using the
source Readable stream's `unpipe()` method.

## Class: stream.Duplex

<!--type=class-->

A "duplex" stream is one that is both Readable and Writable, such as a
TCP socket connection.

Note that `stream.Duplex` is an abstract class designed to be
extended with an underlying implementation of the `_read(size)`
and `_write(chunk, encoding, callback)` methods as you would with a Readable or
Writable stream class.

Since JavaScript doesn't have multiple prototypal inheritance, this
class prototypally inherits from Readable, and then parasitically from
Writable.  It is thus up to the user to implement both the lowlevel
`_read(n)` method as well as the lowlevel `_write(chunk, encoding, cb)` method
on extension duplex classes.

### new stream.Duplex(options)

* `options` {Object} Passed to both Writable and Readable
  constructors. Also has the following fields:
  * `allowHalfOpen` {Boolean} Default=true.  If set to `false`, then
    the stream will automatically end the readable side when the
    writable side ends and vice versa.

In classes that extend the Duplex class, make sure to call the
constructor so that the buffering settings can be properly
initialized.

## Class: stream.Transform

A "transform" stream is a duplex stream where the output is causally
connected in some way to the input, such as a zlib stream or a crypto
stream.

There is no requirement that the output be the same size as the input,
the same number of chunks, or arrive at the same time.  For example, a
Hash stream will only ever have a single chunk of output which is
provided when the input is ended.  A zlib stream will either produce
much smaller or much larger than its input.

Rather than implement the `_read()` and `_write()` methods, Transform
classes must implement the `_transform()` method, and may optionally
also implement the `_flush()` method.  (See below.)

### new stream.Transform([options])

* `options` {Object} Passed to both Writable and Readable
  constructors.

In classes that extend the Transform class, make sure to call the
constructor so that the buffering settings can be properly
initialized.

### transform.\_transform(chunk, encoding, callback)

* `chunk` {Buffer | String} The chunk to be transformed.  Will always
  be a buffer unless the `decodeStrings` option was set to `false`.
* `encoding` {String} If the chunk is a string, then this is the
  encoding type.  (Ignore if `decodeStrings` chunk is a buffer.)
* `callback` {Function} Call this function (optionally with an error
  argument) when you are done processing the supplied chunk.

Note: **This function MUST NOT be called directly.**  It should be
implemented by child classes, and called by the internal Transform
class methods only.

All Transform stream implementations must provide a `_transform`
method to accept input and produce output.

`_transform` should do whatever has to be done in this specific
Transform class, to handle the bytes being written, and pass them off
to the readable portion of the interface.  Do asynchronous I/O,
process things, and so on.

Call `transform.push(outputChunk)` 0 or more times to generate output
from this input chunk, depending on how much data you want to output
as a result of this chunk.

Call the callback function only when the current chunk is completely
consumed.  Note that there may or may not be output as a result of any
particular input chunk.

This method is prefixed with an underscore because it is internal to
the class that defines it, and should not be called directly by user
programs.  However, you **are** expected to override this method in
your own extension classes.

### transform.\_flush(callback)

* `callback` {Function} Call this function (optionally with an error
  argument) when you are done flushing any remaining data.

Note: **This function MUST NOT be called directly.**  It MAY be implemented
by child classes, and if so, will be called by the internal Transform
class methods only.

In some cases, your transform operation may need to emit a bit more
data at the end of the stream.  For example, a `Zlib` compression
stream will store up some internal state so that it can optimally
compress the output.  At the end, however, it needs to do the best it
can with what is left, so that the data will be complete.

In those cases, you can implement a `_flush` method, which will be
called at the very end, after all the written data is consumed, but
before emitting `end` to signal the end of the readable side.  Just
like with `_transform`, call `transform.push(chunk)` zero or more
times, as appropriate, and call `callback` when the flush operation is
complete.

This method is prefixed with an underscore because it is internal to
the class that defines it, and should not be called directly by user
programs.  However, you **are** expected to override this method in
your own extension classes.

### Example: `SimpleProtocol` parser

The example above of a simple protocol parser can be implemented much
more simply by using the higher level `Transform` stream class.

In this example, rather than providing the input as an argument, it
would be piped into the parser, which is a more idiomatic Node stream
approach.

```javascript
function SimpleProtocol(options) {
  if (!(this instanceof SimpleProtocol))
    return new SimpleProtocol(options);

  Transform.call(this, options);
  this._inBody = false;
  this._sawFirstCr = false;
  this._rawHeader = [];
  this.header = null;
}

SimpleProtocol.prototype = Object.create(
  Transform.prototype, { constructor: { value: SimpleProtocol }});

SimpleProtocol.prototype._transform = function(chunk, encoding, done) {
  if (!this._inBody) {
    // check if the chunk has a \n\n
    var split = -1;
    for (var i = 0; i < chunk.length; i++) {
      if (chunk[i] === 10) { // '\n'
        if (this._sawFirstCr) {
          split = i;
          break;
        } else {
          this._sawFirstCr = true;
        }
      } else {
        this._sawFirstCr = false;
      }
    }

    if (split === -1) {
      // still waiting for the \n\n
      // stash the chunk, and try again.
      this._rawHeader.push(chunk);
    } else {
      this._inBody = true;
      var h = chunk.slice(0, split);
      this._rawHeader.push(h);
      var header = Buffer.concat(this._rawHeader).toString();
      try {
        this.header = JSON.parse(header);
      } catch (er) {
        this.emit('error', new Error('invalid simple protocol data'));
        return;
      }
      // and let them know that we are done parsing the header.
      this.emit('header', this.header);

      // now, because we got some extra data, emit this first.
      this.push(b);
    }
  } else {
    // from there on, just provide the data to our consumer as-is.
    this.push(b);
  }
  done();
};

var parser = new SimpleProtocol();
source.pipe(parser)

// Now parser is a readable stream that will emit 'header'
// with the parsed header data.
```


## Class: stream.PassThrough

This is a trivial implementation of a `Transform` stream that simply
passes the input bytes across to the output.  Its purpose is mainly
for examples and testing, but there are occasionally use cases where
it can come in handy.


[EventEmitter]: events.html#events_class_events_eventemitter
