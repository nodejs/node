# Stream

<!--introduced_in=v0.10.0-->

> Stability: 2 - Stable

A stream is an abstract interface for working with streaming data in Node.js.
The `stream` module provides a base API that makes it easy to build objects
that implement the stream interface.

There are many stream objects provided by Node.js. For instance, a
[request to an HTTP server][http-incoming-message] and [`process.stdout`][]
are both stream instances.

Streams can be readable, writable, or both. All streams are instances of
[`EventEmitter`][].

The `stream` module can be accessed using:

```js
const stream = require('stream');
```

While it is important for all Node.js users to understand how streams work,
the `stream` module itself is most useful for developers that are creating new
types of stream instances. Developers who are primarily *consuming* stream
objects will rarely (if ever) have need to use the `stream` module directly.

## Organization of this Document

This document is divided into two primary sections with a third section for
additional notes. The first section explains the elements of the stream API that
are required to *use* streams within an application. The second section explains
the elements of the API that are required to *implement* new types of streams.

## Types of Streams

There are four fundamental stream types within Node.js:

* [Readable][] - streams from which data can be read (for example
  [`fs.createReadStream()`][]).
* [Writable][] - streams to which data can be written (for example
  [`fs.createWriteStream()`][]).
* [Duplex][] - streams that are both Readable and Writable (for example
  [`net.Socket`][]).
* [Transform][] - Duplex streams that can modify or transform the data as it
  is written and read (for example [`zlib.createDeflate()`][]).

### Object Mode

All streams created by Node.js APIs operate exclusively on strings and `Buffer`
(or `Uint8Array`) objects. It is possible, however, for stream implementations
to work with other types of JavaScript values (with the exception of `null`,
which serves a special purpose within streams). Such streams are considered to
operate in "object mode".

Stream instances are switched into object mode using the `objectMode` option
when the stream is created. Attempting to switch an existing stream into
object mode is not safe.

### Buffering

<!--type=misc-->

Both [Writable][] and [Readable][] streams will store data in an internal
buffer that can be retrieved using `writable._writableState.getBuffer()` or
`readable._readableState.buffer`, respectively.

The amount of data potentially buffered depends on the `highWaterMark` option
passed into the streams constructor. For normal streams, the `highWaterMark`
option specifies a total number of bytes. For streams operating in object mode,
the `highWaterMark` specifies a total number of objects.

Data is buffered in Readable streams when the implementation calls
[`stream.push(chunk)`][stream-push]. If the consumer of the Stream does not
call [`stream.read()`][stream-read], the data will sit in the internal
queue until it is consumed.

Once the total size of the internal read buffer reaches the threshold specified
by `highWaterMark`, the stream will temporarily stop reading data from the
underlying resource until the data currently buffered can be consumed (that is,
the stream will stop calling the internal `readable._read()` method that is
used to fill the read buffer).

Data is buffered in Writable streams when the
[`writable.write(chunk)`][stream-write] method is called repeatedly. While the
total size of the internal write buffer is below the threshold set by
`highWaterMark`, calls to `writable.write()` will return `true`. Once
the size of the internal buffer reaches or exceeds the `highWaterMark`, `false`
will be returned.

A key goal of the `stream` API, particularly the [`stream.pipe()`] method,
is to limit the buffering of data to acceptable levels such that sources and
destinations of differing speeds will not overwhelm the available memory.

Because [Duplex][] and [Transform][] streams are both Readable and Writable,
each maintain *two* separate internal buffers used for reading and writing,
allowing each side to operate independently of the other while maintaining an
appropriate and efficient flow of data. For example, [`net.Socket`][] instances
are [Duplex][] streams whose Readable side allows consumption of data received
*from* the socket and whose Writable side allows writing data *to* the socket.
Because data may be written to the socket at a faster or slower rate than data
is received, it is important for each side to operate (and buffer) independently
of the other.

## API for Stream Consumers

<!--type=misc-->

Almost all Node.js applications, no matter how simple, use streams in some
manner. The following is an example of using streams in a Node.js application
that implements an HTTP server:

```js
const http = require('http');

const server = http.createServer((req, res) => {
  // req is an http.IncomingMessage, which is a Readable Stream
  // res is an http.ServerResponse, which is a Writable Stream

  let body = '';
  // Get the data as utf8 strings.
  // If an encoding is not set, Buffer objects will be received.
  req.setEncoding('utf8');

  // Readable streams emit 'data' events once a listener is added
  req.on('data', (chunk) => {
    body += chunk;
  });

  // the end event indicates that the entire body has been received
  req.on('end', () => {
    try {
      const data = JSON.parse(body);
      // write back something interesting to the user:
      res.write(typeof data);
      res.end();
    } catch (er) {
      // uh oh! bad json!
      res.statusCode = 400;
      return res.end(`error: ${er.message}`);
    }
  });
});

server.listen(1337);

// $ curl localhost:1337 -d "{}"
// object
// $ curl localhost:1337 -d "\"foo\""
// string
// $ curl localhost:1337 -d "not json"
// error: Unexpected token o in JSON at position 1
```

[Writable][] streams (such as `res` in the example) expose methods such as
`write()` and `end()` that are used to write data onto the stream.

[Readable][] streams use the [`EventEmitter`][] API for notifying application
code when data is available to be read off the stream. That available data can
be read from the stream in multiple ways.

Both [Writable][] and [Readable][] streams use the [`EventEmitter`][] API in
various ways to communicate the current state of the stream.

[Duplex][] and [Transform][] streams are both [Writable][] and [Readable][].

Applications that are either writing data to or consuming data from a stream
are not required to implement the stream interfaces directly and will generally
have no reason to call `require('stream')`.

Developers wishing to implement new types of streams should refer to the
section [API for Stream Implementers][].

### Writable Streams

Writable streams are an abstraction for a *destination* to which data is
written.

Examples of [Writable][] streams include:

* [HTTP requests, on the client][]
* [HTTP responses, on the server][]
* [fs write streams][]
* [zlib streams][zlib]
* [crypto streams][crypto]
* [TCP sockets][]
* [child process stdin][]
* [`process.stdout`][], [`process.stderr`][]

*Note*: Some of these examples are actually [Duplex][] streams that implement
the [Writable][] interface.

All [Writable][] streams implement the interface defined by the
`stream.Writable` class.

While specific instances of [Writable][] streams may differ in various ways,
all Writable streams follow the same fundamental usage pattern as illustrated
in the example below:

```js
const myStream = getWritableStreamSomehow();
myStream.write('some data');
myStream.write('some more data');
myStream.end('done writing data');
```

#### Class: stream.Writable
<!-- YAML
added: v0.9.4
-->

<!--type=class-->

##### Event: 'close'
<!-- YAML
added: v0.9.4
-->

The `'close'` event is emitted when the stream and any of its underlying
resources (a file descriptor, for example) have been closed. The event indicates
that no more events will be emitted, and no further computation will occur.

Not all Writable streams will emit the `'close'` event.

##### Event: 'drain'
<!-- YAML
added: v0.9.4
-->

If a call to [`stream.write(chunk)`][stream-write] returns `false`, the
`'drain'` event will be emitted when it is appropriate to resume writing data
to the stream.

```js
// Write the data to the supplied writable stream one million times.
// Be attentive to back-pressure.
function writeOneMillionTimes(writer, data, encoding, callback) {
  let i = 1000000;
  write();
  function write() {
    let ok = true;
    do {
      i--;
      if (i === 0) {
        // last time!
        writer.write(data, encoding, callback);
      } else {
        // see if we should continue, or wait
        // don't pass the callback, because we're not done yet.
        ok = writer.write(data, encoding);
      }
    } while (i > 0 && ok);
    if (i > 0) {
      // had to stop early!
      // write some more once it drains
      writer.once('drain', write);
    }
  }
}
```

##### Event: 'error'
<!-- YAML
added: v0.9.4
-->

* {Error}

The `'error'` event is emitted if an error occurred while writing or piping
data. The listener callback is passed a single `Error` argument when called.

*Note*: The stream is not closed when the `'error'` event is emitted.

##### Event: 'finish'
<!-- YAML
added: v0.9.4
-->

The `'finish'` event is emitted after the [`stream.end()`][stream-end] method
has been called, and all data has been flushed to the underlying system.

```js
const writer = getWritableStreamSomehow();
for (let i = 0; i < 100; i++) {
  writer.write(`hello, #${i}!\n`);
}
writer.end('This is the end\n');
writer.on('finish', () => {
  console.error('All writes are now complete.');
});
```

##### Event: 'pipe'
<!-- YAML
added: v0.9.4
-->

* `src` {stream.Readable} source stream that is piping to this writable

The `'pipe'` event is emitted when the [`stream.pipe()`][] method is called on
a readable stream, adding this writable to its set of destinations.

```js
const writer = getWritableStreamSomehow();
const reader = getReadableStreamSomehow();
writer.on('pipe', (src) => {
  console.error('something is piping into the writer');
  assert.equal(src, reader);
});
reader.pipe(writer);
```

##### Event: 'unpipe'
<!-- YAML
added: v0.9.4
-->

* `src` {[Readable][] Stream} The source stream that
  [unpiped][`stream.unpipe()`] this writable

The `'unpipe'` event is emitted when the [`stream.unpipe()`][] method is called
on a [Readable][] stream, removing this [Writable][] from its set of
destinations.

```js
const writer = getWritableStreamSomehow();
const reader = getReadableStreamSomehow();
writer.on('unpipe', (src) => {
  console.error('Something has stopped piping into the writer.');
  assert.equal(src, reader);
});
reader.pipe(writer);
reader.unpipe(writer);
```

##### writable.cork()
<!-- YAML
added: v0.11.2
-->

The `writable.cork()` method forces all written data to be buffered in memory.
The buffered data will be flushed when either the [`stream.uncork()`][] or
[`stream.end()`][stream-end] methods are called.

The primary intent of `writable.cork()` is to avoid a situation where writing
many small chunks of data to a stream do not cause a backup in the internal
buffer that would have an adverse impact on performance. In such situations,
implementations that implement the `writable._writev()` method can perform
buffered writes in a more optimized manner.

See also: [`writable.uncork()`][].

##### writable.end([chunk][, encoding][, callback])
<!-- YAML
added: v0.9.4
changes:
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/11608
    description: The `chunk` argument can now be a `Uint8Array` instance.
-->

* `chunk` {string|Buffer|Uint8Array|any} Optional data to write. For streams
  not operating in object mode, `chunk` must be a string, `Buffer` or
  `Uint8Array`. For object mode streams, `chunk` may be any JavaScript value
  other than `null`.
* `encoding` {string} The encoding, if `chunk` is a string
* `callback` {Function} Optional callback for when the stream is finished

Calling the `writable.end()` method signals that no more data will be written
to the [Writable][]. The optional `chunk` and `encoding` arguments allow one
final additional chunk of data to be written immediately before closing the
stream. If provided, the optional `callback` function is attached as a listener
for the [`'finish'`][] event.

Calling the [`stream.write()`][stream-write] method after calling
[`stream.end()`][stream-end] will raise an error.

```js
// write 'hello, ' and then end with 'world!'
const file = fs.createWriteStream('example.txt');
file.write('hello, ');
file.end('world!');
// writing more now is not allowed!
```

##### writable.setDefaultEncoding(encoding)
<!-- YAML
added: v0.11.15
changes:
  - version: v6.1.0
    pr-url: https://github.com/nodejs/node/pull/5040
    description: This method now returns a reference to `writable`.
-->

* `encoding` {string} The new default encoding
* Returns: `this`

The `writable.setDefaultEncoding()` method sets the default `encoding` for a
[Writable][] stream.

##### writable.uncork()
<!-- YAML
added: v0.11.2
-->

The `writable.uncork()` method flushes all data buffered since
[`stream.cork()`][] was called.

When using [`writable.cork()`][] and `writable.uncork()` to manage the buffering
of writes to a stream, it is recommended that calls to `writable.uncork()` be
deferred using `process.nextTick()`. Doing so allows batching of all
`writable.write()` calls that occur within a given Node.js event loop phase.

```js
stream.cork();
stream.write('some ');
stream.write('data ');
process.nextTick(() => stream.uncork());
```

If the [`writable.cork()`][] method is called multiple times on a stream, the same
number of calls to `writable.uncork()` must be called to flush the buffered
data.

```js
stream.cork();
stream.write('some ');
stream.cork();
stream.write('data ');
process.nextTick(() => {
  stream.uncork();
  // The data will not be flushed until uncork() is called a second time.
  stream.uncork();
});
```

See also: [`writable.cork()`][].

##### writable.write(chunk[, encoding][, callback])
<!-- YAML
added: v0.9.4
changes:
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/11608
    description: The `chunk` argument can now be a `Uint8Array` instance.
  - version: v6.0.0
    pr-url: https://github.com/nodejs/node/pull/6170
    description: Passing `null` as the `chunk` parameter will always be
                 considered invalid now, even in object mode.
-->

* `chunk` {string|Buffer|Uint8Array|any} Optional data to write. For streams
  not operating in object mode, `chunk` must be a string, `Buffer` or
  `Uint8Array`. For object mode streams, `chunk` may be any JavaScript value
  other than `null`.
* `encoding` {string} The encoding, if `chunk` is a string
* `callback` {Function} Callback for when this chunk of data is flushed
* Returns: {boolean} `false` if the stream wishes for the calling code to
  wait for the `'drain'` event to be emitted before continuing to write
  additional data; otherwise `true`.

The `writable.write()` method writes some data to the stream, and calls the
supplied `callback` once the data has been fully handled. If an error
occurs, the `callback` *may or may not* be called with the error as its
first argument. To reliably detect write errors, add a listener for the
`'error'` event.

The return value is `true` if the internal buffer is less than the
`highWaterMark` configured when the stream was created after admitting `chunk`.
If `false` is returned, further attempts to write data to the stream should
stop until the [`'drain'`][] event is emitted.

While a stream is not draining, calls to `write()` will buffer `chunk`, and
return false. Once all currently buffered chunks are drained (accepted for
delivery by the operating system), the `'drain'` event will be emitted.
It is recommended that once write() returns false, no more chunks be written
until the `'drain'` event is emitted. While calling `write()` on a stream that
is not draining is allowed, Node.js will buffer all written chunks until
maximum memory usage occurs, at which point it will abort unconditionally.
Even before it aborts, high memory usage will cause poor garbage collector
performance and high RSS (which is not typically released back to the system,
even after the memory is no longer required). Since TCP sockets may never
drain if the remote peer does not read the data, writing a socket that is
not draining may lead to a remotely exploitable vulnerability.

Writing data while the stream is not draining is particularly
problematic for a [Transform][], because the `Transform` streams are paused
by default until they are piped or an `'data'` or `'readable'` event handler
is added.

If the data to be written can be generated or fetched on demand, it is
recommended to encapsulate the logic into a [Readable][] and use
[`stream.pipe()`][]. However, if calling `write()` is preferred, it is
possible to respect backpressure and avoid memory issues using the
[`'drain'`][] event:

```js
function write(data, cb) {
  if (!stream.write(data)) {
    stream.once('drain', cb);
  } else {
    process.nextTick(cb);
  }
}

// Wait for cb to be called before doing any other write.
write('hello', () => {
  console.log('write completed, do more writes now');
});
```

A Writable stream in object mode will always ignore the `encoding` argument.

##### writable.destroy([error])
<!-- YAML
added: v8.0.0
-->

* Returns: `this`

Destroy the stream, and emit the passed error. After this call, the
writable stream has ended. Implementors should not override this method,
but instead implement [`writable._destroy`][writable-_destroy].

### Readable Streams

Readable streams are an abstraction for a *source* from which data is
consumed.

Examples of Readable streams include:

* [HTTP responses, on the client][http-incoming-message]
* [HTTP requests, on the server][http-incoming-message]
* [fs read streams][]
* [zlib streams][zlib]
* [crypto streams][crypto]
* [TCP sockets][]
* [child process stdout and stderr][]
* [`process.stdin`][]

All [Readable][] streams implement the interface defined by the
`stream.Readable` class.

#### Two Modes

Readable streams effectively operate in one of two modes: flowing and paused.

When in flowing mode, data is read from the underlying system automatically
and provided to an application as quickly as possible using events via the
[`EventEmitter`][] interface.

In paused mode, the [`stream.read()`][stream-read] method must be called
explicitly to read chunks of data from the stream.

All [Readable][] streams begin in paused mode but can be switched to flowing
mode in one of the following ways:

* Adding a [`'data'`][] event handler.
* Calling the [`stream.resume()`][stream-resume] method.
* Calling the [`stream.pipe()`][] method to send the data to a [Writable][].

The Readable can switch back to paused mode using one of the following:

* If there are no pipe destinations, by calling the
  [`stream.pause()`][stream-pause] method.
* If there are pipe destinations, by removing any [`'data'`][] event
  handlers, and removing all pipe destinations by calling the
  [`stream.unpipe()`][] method.

The important concept to remember is that a Readable will not generate data
until a mechanism for either consuming or ignoring that data is provided. If
the consuming mechanism is disabled or taken away, the Readable will *attempt*
to stop generating the data.

*Note*: For backwards compatibility reasons, removing [`'data'`][] event
handlers will **not** automatically pause the stream. Also, if there are piped
destinations, then calling [`stream.pause()`][stream-pause] will not guarantee
that the stream will *remain* paused once those destinations drain and ask for
more data.

*Note*: If a [Readable][] is switched into flowing mode and there are no
consumers available to handle the data, that data will be lost. This can occur,
for instance, when the `readable.resume()` method is called without a listener
attached to the `'data'` event, or when a `'data'` event handler is removed
from the stream.

#### Three States

The "two modes" of operation for a Readable stream are a simplified abstraction
for the more complicated internal state management that is happening within the
Readable stream implementation.

Specifically, at any given point in time, every Readable is in one of three
possible states:

* `readable._readableState.flowing = null`
* `readable._readableState.flowing = false`
* `readable._readableState.flowing = true`

When `readable._readableState.flowing` is `null`, no mechanism for consuming the
streams data is provided so the stream will not generate its data. While in this
state, attaching a listener for the `'data'` event, calling the `readable.pipe()`
method, or calling the `readable.resume()` method will switch
`readable._readableState.flowing` to `true`, causing the Readable to begin
actively emitting events as data is generated.

Calling `readable.pause()`, `readable.unpipe()`, or receiving "back pressure"
will cause the `readable._readableState.flowing` to be set as `false`,
temporarily halting the flowing of events but *not* halting the generation of
data. While in this state, attaching a listener for the `'data'` event
would not cause `readable._readableState.flowing` to switch to `true`.

```js
const { PassThrough, Writable } = require('stream');
const pass = new PassThrough();
const writable = new Writable();

pass.pipe(writable);
pass.unpipe(writable);
// flowing is now false

pass.on('data', (chunk) => { console.log(chunk.toString()); });
pass.write('ok'); // will not emit 'data'
pass.resume(); // must be called to make 'data' being emitted
```

While `readable._readableState.flowing` is `false`, data may be accumulating
within the streams internal buffer.

#### Choose One

The Readable stream API evolved across multiple Node.js versions and provides
multiple methods of consuming stream data. In general, developers should choose
*one* of the methods of consuming data and *should never* use multiple methods
to consume data from a single stream.

Use of the `readable.pipe()` method is recommended for most users as it has been
implemented to provide the easiest way of consuming stream data. Developers that
require more fine-grained control over the transfer and generation of data can
use the [`EventEmitter`][] and `readable.pause()`/`readable.resume()` APIs.

#### Class: stream.Readable
<!-- YAML
added: v0.9.4
-->

<!--type=class-->

##### Event: 'close'
<!-- YAML
added: v0.9.4
-->

The `'close'` event is emitted when the stream and any of its underlying
resources (a file descriptor, for example) have been closed. The event indicates
that no more events will be emitted, and no further computation will occur.

Not all [Readable][] streams will emit the `'close'` event.

##### Event: 'data'
<!-- YAML
added: v0.9.4
-->

* `chunk` {Buffer|string|any} The chunk of data. For streams that are not
  operating in object mode, the chunk will be either a string or `Buffer`.
  For streams that are in object mode, the chunk can be any JavaScript value
  other than `null`.

The `'data'` event is emitted whenever the stream is relinquishing ownership of
a chunk of data to a consumer. This may occur whenever the stream is switched
in flowing mode by calling `readable.pipe()`, `readable.resume()`, or by
attaching a listener callback to the `'data'` event. The `'data'` event will
also be emitted whenever the `readable.read()` method is called and a chunk of
data is available to be returned.

Attaching a `'data'` event listener to a stream that has not been explicitly
paused will switch the stream into flowing mode. Data will then be passed as
soon as it is available.

The listener callback will be passed the chunk of data as a string if a default
encoding has been specified for the stream using the
`readable.setEncoding()` method; otherwise the data will be passed as a
`Buffer`.

```js
const readable = getReadableStreamSomehow();
readable.on('data', (chunk) => {
  console.log(`Received ${chunk.length} bytes of data.`);
});
```

##### Event: 'end'
<!-- YAML
added: v0.9.4
-->

The `'end'` event is emitted when there is no more data to be consumed from
the stream.

*Note*: The `'end'` event **will not be emitted** unless the data is
completely consumed. This can be accomplished by switching the stream into
flowing mode, or by calling [`stream.read()`][stream-read] repeatedly until
all data has been consumed.

```js
const readable = getReadableStreamSomehow();
readable.on('data', (chunk) => {
  console.log(`Received ${chunk.length} bytes of data.`);
});
readable.on('end', () => {
  console.log('There will be no more data.');
});
```

##### Event: 'error'
<!-- YAML
added: v0.9.4
-->

* {Error}

The `'error'` event may be emitted by a Readable implementation at any time.
Typically, this may occur if the underlying stream is unable to generate data
due to an underlying internal failure, or when a stream implementation attempts
to push an invalid chunk of data.

The listener callback will be passed a single `Error` object.

##### Event: 'readable'
<!-- YAML
added: v0.9.4
-->

The `'readable'` event is emitted when there is data available to be read from
the stream. In some cases, attaching a listener for the `'readable'` event will
cause some amount of data to be read into an internal buffer.

```javascript
const readable = getReadableStreamSomehow();
readable.on('readable', () => {
  // there is some data to read now
});
```
The `'readable'` event will also be emitted once the end of the stream data
has been reached but before the `'end'` event is emitted.

Effectively, the `'readable'` event indicates that the stream has new
information: either new data is available or the end of the stream has been
reached. In the former case, [`stream.read()`][stream-read] will return the
available data. In the latter case, [`stream.read()`][stream-read] will return
`null`. For instance, in the following example, `foo.txt` is an empty file:

```js
const fs = require('fs');
const rr = fs.createReadStream('foo.txt');
rr.on('readable', () => {
  console.log('readable:', rr.read());
});
rr.on('end', () => {
  console.log('end');
});
```

The output of running this script is:

```txt
$ node test.js
readable: null
end
```

*Note*: In general, the `readable.pipe()` and `'data'` event mechanisms are
easier to understand than the `'readable'` event.
However, handling `'readable'` might result in increased throughput.

##### readable.isPaused()
<!-- YAML
added: v0.11.14
-->

* Returns: {boolean}

The `readable.isPaused()` method returns the current operating state of the
Readable. This is used primarily by the mechanism that underlies the
`readable.pipe()` method. In most typical cases, there will be no reason to
use this method directly.

```js
const readable = new stream.Readable();

readable.isPaused(); // === false
readable.pause();
readable.isPaused(); // === true
readable.resume();
readable.isPaused(); // === false
```

##### readable.pause()
<!-- YAML
added: v0.9.4
-->

* Returns: `this`

The `readable.pause()` method will cause a stream in flowing mode to stop
emitting [`'data'`][] events, switching out of flowing mode. Any data that
becomes available will remain in the internal buffer.

```js
const readable = getReadableStreamSomehow();
readable.on('data', (chunk) => {
  console.log(`Received ${chunk.length} bytes of data.`);
  readable.pause();
  console.log('There will be no additional data for 1 second.');
  setTimeout(() => {
    console.log('Now data will start flowing again.');
    readable.resume();
  }, 1000);
});
```

##### readable.pipe(destination[, options])
<!-- YAML
added: v0.9.4
-->

* `destination` {stream.Writable} The destination for writing data
* `options` {Object} Pipe options
  * `end` {boolean} End the writer when the reader ends. Defaults to `true`.

The `readable.pipe()` method attaches a [Writable][] stream to the `readable`,
causing it to switch automatically into flowing mode and push all of its data
to the attached [Writable][]. The flow of data will be automatically managed so
that the destination Writable stream is not overwhelmed by a faster Readable
stream.

The following example pipes all of the data from the `readable` into a file
named `file.txt`:

```js
const readable = getReadableStreamSomehow();
const writable = fs.createWriteStream('file.txt');
// All the data from readable goes into 'file.txt'
readable.pipe(writable);
```
It is possible to attach multiple Writable streams to a single Readable stream.

The `readable.pipe()` method returns a reference to the *destination* stream
making it possible to set up chains of piped streams:

```js
const r = fs.createReadStream('file.txt');
const z = zlib.createGzip();
const w = fs.createWriteStream('file.txt.gz');
r.pipe(z).pipe(w);
```

By default, [`stream.end()`][stream-end] is called on the destination Writable
stream when the source Readable stream emits [`'end'`][], so that the
destination is no longer writable. To disable this default behavior, the `end`
option can be passed as `false`, causing the destination stream to remain open,
as illustrated in the following example:

```js
reader.pipe(writer, { end: false });
reader.on('end', () => {
  writer.end('Goodbye\n');
});
```

One important caveat is that if the Readable stream emits an error during
processing, the Writable destination *is not closed* automatically. If an
error occurs, it will be necessary to *manually* close each stream in order
to prevent memory leaks.

*Note*: The [`process.stderr`][] and [`process.stdout`][] Writable streams are
never closed until the Node.js process exits, regardless of the specified
options.

##### readable.read([size])
<!-- YAML
added: v0.9.4
-->

* `size` {number} Optional argument to specify how much data to read.
* Return {string|Buffer|null}

The `readable.read()` method pulls some data out of the internal buffer and
returns it. If no data available to be read, `null` is returned. By default,
the data will be returned as a `Buffer` object unless an encoding has been
specified using the `readable.setEncoding()` method or the stream is operating
in object mode.

The optional `size` argument specifies a specific number of bytes to read. If
`size` bytes are not available to be read, `null` will be returned *unless*
the stream has ended, in which case all of the data remaining in the internal
buffer will be returned.

If the `size` argument is not specified, all of the data contained in the
internal buffer will be returned.

The `readable.read()` method should only be called on Readable streams operating
in paused mode. In flowing mode, `readable.read()` is called automatically until
the internal buffer is fully drained.

```js
const readable = getReadableStreamSomehow();
readable.on('readable', () => {
  let chunk;
  while (null !== (chunk = readable.read())) {
    console.log(`Received ${chunk.length} bytes of data.`);
  }
});
```

In general, it is recommended that developers avoid the use of the `'readable'`
event and the `readable.read()` method in favor of using either
`readable.pipe()` or the `'data'` event.

A Readable stream in object mode will always return a single item from
a call to [`readable.read(size)`][stream-read], regardless of the value of the
`size` argument.

*Note*: If the `readable.read()` method returns a chunk of data, a `'data'`
event will also be emitted.

*Note*: Calling [`stream.read([size])`][stream-read] after the [`'end'`][]
event has been emitted will return `null`. No runtime error will be raised.

##### readable.resume()
<!-- YAML
added: v0.9.4
-->

* Returns: `this`

The `readable.resume()` method causes an explicitly paused Readable stream to
resume emitting [`'data'`][] events, switching the stream into flowing mode.

The `readable.resume()` method can be used to fully consume the data from a
stream without actually processing any of that data as illustrated in the
following example:

```js
getReadableStreamSomehow()
  .resume()
  .on('end', () => {
    console.log('Reached the end, but did not read anything.');
  });
```

##### readable.setEncoding(encoding)
<!-- YAML
added: v0.9.4
-->

* `encoding` {string} The encoding to use.
* Returns: `this`

The `readable.setEncoding()` method sets the character encoding for
data read from the Readable stream.

By default, no encoding is assigned and stream data will be returned as
`Buffer` objects. Setting an encoding causes the stream data
to be returned as strings of the specified encoding rather than as `Buffer`
objects. For instance, calling `readable.setEncoding('utf8')` will cause the
output data to be interpreted as UTF-8 data, and passed as strings. Calling
`readable.setEncoding('hex')` will cause the data to be encoded in hexadecimal
string format.

The Readable stream will properly handle multi-byte characters delivered through
the stream that would otherwise become improperly decoded if simply pulled from
the stream as `Buffer` objects.

```js
const readable = getReadableStreamSomehow();
readable.setEncoding('utf8');
readable.on('data', (chunk) => {
  assert.equal(typeof chunk, 'string');
  console.log('got %d characters of string data', chunk.length);
});
```

##### readable.unpipe([destination])
<!-- YAML
added: v0.9.4
-->

* `destination` {stream.Writable} Optional specific stream to unpipe

The `readable.unpipe()` method detaches a Writable stream previously attached
using the [`stream.pipe()`][] method.

If the `destination` is not specified, then *all* pipes are detached.

If the `destination` is specified, but no pipe is set up for it, then
the method does nothing.

```js
const readable = getReadableStreamSomehow();
const writable = fs.createWriteStream('file.txt');
// All the data from readable goes into 'file.txt',
// but only for the first second
readable.pipe(writable);
setTimeout(() => {
  console.log('Stop writing to file.txt');
  readable.unpipe(writable);
  console.log('Manually close the file stream');
  writable.end();
}, 1000);
```

##### readable.unshift(chunk)
<!-- YAML
added: v0.9.11
changes:
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/11608
    description: The `chunk` argument can now be a `Uint8Array` instance.
-->

* `chunk` {Buffer|Uint8Array|string|any} Chunk of data to unshift onto the
  read queue. For streams not operating in object mode, `chunk` must be a
  string, `Buffer` or `Uint8Array`. For object mode streams, `chunk` may be
  any JavaScript value other than `null`.

The `readable.unshift()` method pushes a chunk of data back into the internal
buffer. This is useful in certain situations where a stream is being consumed by
code that needs to "un-consume" some amount of data that it has optimistically
pulled out of the source, so that the data can be passed on to some other party.

*Note*: The `stream.unshift(chunk)` method cannot be called after the
[`'end'`][] event has been emitted or a runtime error will be thrown.

Developers using `stream.unshift()` often should consider switching to
use of a [Transform][] stream instead. See the [API for Stream Implementers][]
section for more information.

```js
// Pull off a header delimited by \n\n
// use unshift() if we get too much
// Call the callback with (error, header, stream)
const { StringDecoder } = require('string_decoder');
function parseHeader(stream, callback) {
  stream.on('error', callback);
  stream.on('readable', onReadable);
  const decoder = new StringDecoder('utf8');
  let header = '';
  function onReadable() {
    let chunk;
    while (null !== (chunk = stream.read())) {
      const str = decoder.write(chunk);
      if (str.match(/\n\n/)) {
        // found the header boundary
        const split = str.split(/\n\n/);
        header += split.shift();
        const remaining = split.join('\n\n');
        const buf = Buffer.from(remaining, 'utf8');
        stream.removeListener('error', callback);
        // remove the readable listener before unshifting
        stream.removeListener('readable', onReadable);
        if (buf.length)
          stream.unshift(buf);
        // now the body of the message can be read from the stream.
        callback(null, header, stream);
      } else {
        // still reading the header.
        header += str;
      }
    }
  }
}
```

*Note*: Unlike [`stream.push(chunk)`][stream-push], `stream.unshift(chunk)`
will not end the reading process by resetting the internal reading state of the
stream. This can cause unexpected results if `readable.unshift()` is called
during a read (i.e. from within a [`stream._read()`][stream-_read]
implementation on a custom stream). Following the call to `readable.unshift()`
with an immediate [`stream.push('')`][stream-push] will reset the reading state
appropriately, however it is best to simply avoid calling `readable.unshift()`
while in the process of performing a read.

##### readable.wrap(stream)
<!-- YAML
added: v0.9.4
-->

* `stream` {Stream} An "old style" readable stream

Versions of Node.js prior to v0.10 had streams that did not implement the
entire `stream` module API as it is currently defined. (See [Compatibility][]
for more information.)

When using an older Node.js library that emits [`'data'`][] events and has a
[`stream.pause()`][stream-pause] method that is advisory only, the
`readable.wrap()` method can be used to create a [Readable][] stream that uses
the old stream as its data source.

It will rarely be necessary to use `readable.wrap()` but the method has been
provided as a convenience for interacting with older Node.js applications and
libraries.

For example:

```js
const { OldReader } = require('./old-api-module.js');
const { Readable } = require('stream');
const oreader = new OldReader();
const myReader = new Readable().wrap(oreader);

myReader.on('readable', () => {
  myReader.read(); // etc.
});
```

##### readable.destroy([error])
<!-- YAML
added: v8.0.0
-->

Destroy the stream, and emit `'error'`. After this call, the
readable stream will release any internal resources.
Implementors should not override this method, but instead implement
[`readable._destroy`][readable-_destroy].

### Duplex and Transform Streams

#### Class: stream.Duplex
<!-- YAML
added: v0.9.4
changes:
  - version: v6.8.0
    pr-url: https://github.com/nodejs/node/pull/8834
    description: Instances of `Duplex` now return `true` when
                 checking `instanceof stream.Writable`.
-->

<!--type=class-->

Duplex streams are streams that implement both the [Readable][] and
[Writable][] interfaces.

Examples of Duplex streams include:

* [TCP sockets][]
* [zlib streams][zlib]
* [crypto streams][crypto]

#### Class: stream.Transform
<!-- YAML
added: v0.9.4
-->

<!--type=class-->

Transform streams are [Duplex][] streams where the output is in some way
related to the input. Like all [Duplex][] streams, Transform streams
implement both the [Readable][] and [Writable][] interfaces.

Examples of Transform streams include:

* [zlib streams][zlib]
* [crypto streams][crypto]

##### transform.destroy([error])
<!-- YAML
added: v8.0.0
-->

Destroy the stream, and emit `'error'`. After this call, the
transform stream would release any internal resources.
implementors should not override this method, but instead implement
[`readable._destroy`][readable-_destroy].
The default implementation of `_destroy` for `Transform` also emit `'close'`.

## API for Stream Implementers

<!--type=misc-->

The `stream` module API has been designed to make it possible to easily
implement streams using JavaScript's prototypal inheritance model.

First, a stream developer would declare a new JavaScript class that extends one
of the four basic stream classes (`stream.Writable`, `stream.Readable`,
`stream.Duplex`, or `stream.Transform`), making sure they call the appropriate
parent class constructor:

```js
const { Writable } = require('stream');

class MyWritable extends Writable {
  constructor(options) {
    super(options);
    // ...
  }
}
```

The new stream class must then implement one or more specific methods, depending
on the type of stream being created, as detailed in the chart below:

<table>
  <thead>
    <tr>
      <th>
        <p>Use-case</p>
      </th>
      <th>
        <p>Class</p>
      </th>
      <th>
        <p>Method(s) to implement</p>
      </th>
    </tr>
  </thead>
  <tr>
    <td>
      <p>Reading only</p>
    </td>
    <td>
      <p>[Readable](#stream_class_stream_readable)</p>
    </td>
    <td>
      <p><code>[_read][stream-_read]</code></p>
    </td>
  </tr>
  <tr>
    <td>
      <p>Writing only</p>
    </td>
    <td>
      <p>[Writable](#stream_class_stream_writable)</p>
    </td>
    <td>
      <p><code>[_write][stream-_write]</code>, <code>[_writev][stream-_writev]</code>,
      <code>[_final][stream-_final]</code></p>
    </td>
  </tr>
  <tr>
    <td>
      <p>Reading and writing</p>
    </td>
    <td>
      <p>[Duplex](#stream_class_stream_duplex)</p>
    </td>
    <td>
      <p><code>[_read][stream-_read]</code>, <code>[_write][stream-_write]</code>, <code>[_writev][stream-_writev]</code>,
      <code>[_final][stream-_final]</code></p>
    </td>
  </tr>
  <tr>
    <td>
      <p>Operate on written data, then read the result</p>
    </td>
    <td>
      <p>[Transform](#stream_class_stream_transform)</p>
    </td>
    <td>
      <p><code>[_transform][stream-_transform]</code>, <code>[_flush][stream-_flush]</code>,
      <code>[_final][stream-_final]</code></p>
    </td>
  </tr>
</table>

*Note*: The implementation code for a stream should *never* call the "public"
methods of a stream that are intended for use by consumers (as described in
the [API for Stream Consumers][] section). Doing so may lead to adverse
side effects in application code consuming the stream.

### Simplified Construction
<!-- YAML
added: v1.2.0
-->

For many simple cases, it is possible to construct a stream without relying on
inheritance. This can be accomplished by directly creating instances of the
`stream.Writable`, `stream.Readable`, `stream.Duplex` or `stream.Transform`
objects and passing appropriate methods as constructor options.

For example:

```js
const { Writable } = require('stream');

const myWritable = new Writable({
  write(chunk, encoding, callback) {
    // ...
  }
});
```

### Implementing a Writable Stream

The `stream.Writable` class is extended to implement a [Writable][] stream.

Custom Writable streams *must* call the `new stream.Writable([options])`
constructor and implement the `writable._write()` method. The
`writable._writev()` method *may* also be implemented.

#### Constructor: new stream.Writable([options])

* `options` {Object}
  * `highWaterMark` {number} Buffer level when
    [`stream.write()`][stream-write] starts returning `false`. Defaults to
    `16384` (16kb), or `16` for `objectMode` streams.
  * `decodeStrings` {boolean} Whether or not to decode strings into
    Buffers before passing them to [`stream._write()`][stream-_write].
    Defaults to `true`
  * `objectMode` {boolean} Whether or not the
    [`stream.write(anyObj)`][stream-write] is a valid operation. When set,
    it becomes possible to write JavaScript values other than string,
    `Buffer` or `Uint8Array` if supported by the stream implementation.
    Defaults to `false`
  * `write` {Function} Implementation for the
    [`stream._write()`][stream-_write] method.
  * `writev` {Function} Implementation for the
    [`stream._writev()`][stream-_writev] method.
  * `destroy` {Function} Implementation for the
    [`stream._destroy()`][writable-_destroy] method.
  * `final` {Function} Implementation for the
    [`stream._final()`][stream-_final] method.

For example:

```js
const { Writable } = require('stream');

class MyWritable extends Writable {
  constructor(options) {
    // Calls the stream.Writable() constructor
    super(options);
    // ...
  }
}
```

Or, when using pre-ES6 style constructors:

```js
const { Writable } = require('stream');
const util = require('util');

function MyWritable(options) {
  if (!(this instanceof MyWritable))
    return new MyWritable(options);
  Writable.call(this, options);
}
util.inherits(MyWritable, Writable);
```

Or, using the Simplified Constructor approach:

```js
const { Writable } = require('stream');

const myWritable = new Writable({
  write(chunk, encoding, callback) {
    // ...
  },
  writev(chunks, callback) {
    // ...
  }
});
```

#### writable.\_write(chunk, encoding, callback)

* `chunk` {Buffer|string|any} The chunk to be written. Will **always**
  be a buffer unless the `decodeStrings` option was set to `false`
  or the stream is operating in object mode.
* `encoding` {string} If the chunk is a string, then `encoding` is the
  character encoding of that string. If chunk is a `Buffer`, or if the
  stream is operating in object mode, `encoding` may be ignored.
* `callback` {Function} Call this function (optionally with an error
  argument) when processing is complete for the supplied chunk.

All Writable stream implementations must provide a
[`writable._write()`][stream-_write] method to send data to the underlying
resource.

*Note*: [Transform][] streams provide their own implementation of the
[`writable._write()`][stream-_write].

*Note*: This function MUST NOT be called by application code directly. It
should be implemented by child classes, and called by the internal Writable
class methods only.

The `callback` method must be called to signal either that the write completed
successfully or failed with an error. The first argument passed to the
`callback` must be the `Error` object if the call failed or `null` if the
write succeeded.

It is important to note that all calls to `writable.write()` that occur between
the time `writable._write()` is called and the `callback` is called will cause
the written data to be buffered. Once the `callback` is invoked, the stream will
emit a [`'drain'`][] event. If a stream implementation is capable of processing
multiple chunks of data at once, the `writable._writev()` method should be
implemented.

If the `decodeStrings` property is set in the constructor options, then
`chunk` may be a string rather than a Buffer, and `encoding` will
indicate the character encoding of the string. This is to support
implementations that have an optimized handling for certain string
data encodings. If the `decodeStrings` property is explicitly set to `false`,
the `encoding` argument can be safely ignored, and `chunk` will remain the same
object that is passed to `.write()`.

The `writable._write()` method is prefixed with an underscore because it is
internal to the class that defines it, and should never be called directly by
user programs.

#### writable.\_writev(chunks, callback)

* `chunks` {Array} The chunks to be written. Each chunk has following
  format: `{ chunk: ..., encoding: ... }`.
* `callback` {Function} A callback function (optionally with an error
  argument) to be invoked when processing is complete for the supplied chunks.

*Note*: This function MUST NOT be called by application code directly. It
should be implemented by child classes, and called by the internal Writable
class methods only.

The `writable._writev()` method may be implemented in addition to
`writable._write()` in stream implementations that are capable of processing
multiple chunks of data at once. If implemented, the method will be called with
all chunks of data currently buffered in the write queue.

The `writable._writev()` method is prefixed with an underscore because it is
internal to the class that defines it, and should never be called directly by
user programs.

#### writable.\_destroy(err, callback)
<!-- YAML
added: v8.0.0
-->

* `err` {Error} An error.
* `callback` {Function} A callback function that takes an optional error argument
  which is invoked when the writable is destroyed.

#### writable.\_final(callback)
<!-- YAML
added: v8.0.0
-->

* `callback` {Function} Call this function (optionally with an error
  argument) when finished writing any remaining data.

The `_final()` method **must not** be called directly. It may be implemented
by child classes, and if so, will be called by the internal Writable
class methods only.

This optional function will be called before the stream closes, delaying the
`finish` event until `callback` is called. This is useful to close resources
or write buffered data before a stream ends.

#### Errors While Writing

It is recommended that errors occurring during the processing of the
`writable._write()` and `writable._writev()` methods are reported by invoking
the callback and passing the error as the first argument. This will cause an
`'error'` event to be emitted by the Writable. Throwing an Error from within
`writable._write()` can result in unexpected and inconsistent behavior depending
on how the stream is being used.  Using the callback ensures consistent and
predictable handling of errors.

```js
const { Writable } = require('stream');

const myWritable = new Writable({
  write(chunk, encoding, callback) {
    if (chunk.toString().indexOf('a') >= 0) {
      callback(new Error('chunk is invalid'));
    } else {
      callback();
    }
  }
});
```

#### An Example Writable Stream

The following illustrates a rather simplistic (and somewhat pointless) custom
Writable stream implementation. While this specific Writable stream instance
is not of any real particular usefulness, the example illustrates each of the
required elements of a custom [Writable][] stream instance:

```js
const { Writable } = require('stream');

class MyWritable extends Writable {
  constructor(options) {
    super(options);
    // ...
  }

  _write(chunk, encoding, callback) {
    if (chunk.toString().indexOf('a') >= 0) {
      callback(new Error('chunk is invalid'));
    } else {
      callback();
    }
  }
}
```

### Implementing a Readable Stream

The `stream.Readable` class is extended to implement a [Readable][] stream.

Custom Readable streams *must* call the `new stream.Readable([options])`
constructor and implement the `readable._read()` method.

#### new stream.Readable([options])

* `options` {Object}
  * `highWaterMark` {number} The maximum number of bytes to store in
    the internal buffer before ceasing to read from the underlying
    resource. Defaults to `16384` (16kb), or `16` for `objectMode` streams
  * `encoding` {string} If specified, then buffers will be decoded to
    strings using the specified encoding. Defaults to `null`
  * `objectMode` {boolean} Whether this stream should behave
    as a stream of objects. Meaning that [`stream.read(n)`][stream-read] returns
    a single value instead of a Buffer of size n. Defaults to `false`
  * `read` {Function} Implementation for the [`stream._read()`][stream-_read]
    method.
  * `destroy` {Function} Implementation for the [`stream._destroy()`][readable-_destroy]
    method.

For example:

```js
const { Readable } = require('stream');

class MyReadable extends Readable {
  constructor(options) {
    // Calls the stream.Readable(options) constructor
    super(options);
    // ...
  }
}
```

Or, when using pre-ES6 style constructors:

```js
const { Readable } = require('stream');
const util = require('util');

function MyReadable(options) {
  if (!(this instanceof MyReadable))
    return new MyReadable(options);
  Readable.call(this, options);
}
util.inherits(MyReadable, Readable);
```

Or, using the Simplified Constructor approach:

```js
const { Readable } = require('stream');

const myReadable = new Readable({
  read(size) {
    // ...
  }
});
```

#### readable.\_read(size)

* `size` {number} Number of bytes to read asynchronously

*Note*: This function MUST NOT be called by application code directly. It
should be implemented by child classes, and called by the internal Readable
class methods only.

All Readable stream implementations must provide an implementation of the
`readable._read()` method to fetch data from the underlying resource.

When `readable._read()` is called, if data is available from the resource, the
implementation should begin pushing that data into the read queue using the
[`this.push(dataChunk)`][stream-push] method. `_read()` should continue reading
from the resource and pushing data until `readable.push()` returns `false`. Only
when `_read()` is called again after it has stopped should it resume pushing
additional data onto the queue.

*Note*: Once the `readable._read()` method has been called, it will not be
called again until the [`readable.push()`][stream-push] method is called.

The `size` argument is advisory. For implementations where a "read" is a
single operation that returns data can use the `size` argument to determine how
much data to fetch. Other implementations may ignore this argument and simply
provide data whenever it becomes available. There is no need to "wait" until
`size` bytes are available before calling [`stream.push(chunk)`][stream-push].

The `readable._read()` method is prefixed with an underscore because it is
internal to the class that defines it, and should never be called directly by
user programs.

#### readable.push(chunk[, encoding])
<!-- YAML
changes:
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/11608
    description: The `chunk` argument can now be a `Uint8Array` instance.
-->

* `chunk` {Buffer|Uint8Array|string|null|any} Chunk of data to push into the
  read queue. For streams not operating in object mode, `chunk` must be a
  string, `Buffer` or `Uint8Array`. For object mode streams, `chunk` may be
  any JavaScript value.
* `encoding` {string} Encoding of string chunks.  Must be a valid
  Buffer encoding, such as `'utf8'` or `'ascii'`
* Returns {boolean} `true` if additional chunks of data may continued to be
  pushed; `false` otherwise.

When `chunk` is a `Buffer`, `Uint8Array` or `string`, the `chunk` of data will
be added to the internal queue for users of the stream to consume.
Passing `chunk` as `null` signals the end of the stream (EOF), after which no
more data can be written.

When the Readable is operating in paused mode, the data added with
`readable.push()` can be read out by calling the
[`readable.read()`][stream-read] method when the [`'readable'`][] event is
emitted.

When the Readable is operating in flowing mode, the data added with
`readable.push()` will be delivered by emitting a `'data'` event.

The `readable.push()` method is designed to be as flexible as possible. For
example, when wrapping a lower-level source that provides some form of
pause/resume mechanism, and a data callback, the low-level source can be wrapped
by the custom Readable instance as illustrated in the following example:

```js
// source is an object with readStop() and readStart() methods,
// and an `ondata` member that gets called when it has data, and
// an `onend` member that gets called when the data is over.

class SourceWrapper extends Readable {
  constructor(options) {
    super(options);

    this._source = getLowlevelSourceObject();

    // Every time there's data, push it into the internal buffer.
    this._source.ondata = (chunk) => {
      // if push() returns false, then stop reading from source
      if (!this.push(chunk))
        this._source.readStop();
    };

    // When the source ends, push the EOF-signaling `null` chunk
    this._source.onend = () => {
      this.push(null);
    };
  }
  // _read will be called when the stream wants to pull more data in
  // the advisory size argument is ignored in this case.
  _read(size) {
    this._source.readStart();
  }
}
```
*Note*: The `readable.push()` method is intended be called only by Readable
Implementers, and only from within the `readable._read()` method.

#### Errors While Reading

It is recommended that errors occurring during the processing of the
`readable._read()` method are emitted using the `'error'` event rather than
being thrown. Throwing an Error from within `readable._read()` can result in
unexpected and inconsistent behavior depending on whether the stream is
operating in flowing or paused mode. Using the `'error'` event ensures
consistent and predictable handling of errors.

<!-- eslint-disable no-useless-return -->
```js
const { Readable } = require('stream');

const myReadable = new Readable({
  read(size) {
    if (checkSomeErrorCondition()) {
      process.nextTick(() => this.emit('error', err));
      return;
    }
    // do some work
  }
});
```

#### An Example Counting Stream

<!--type=example-->

The following is a basic example of a Readable stream that emits the numerals
from 1 to 1,000,000 in ascending order, and then ends.

```js
const { Readable } = require('stream');

class Counter extends Readable {
  constructor(opt) {
    super(opt);
    this._max = 1000000;
    this._index = 1;
  }

  _read() {
    const i = this._index++;
    if (i > this._max)
      this.push(null);
    else {
      const str = '' + i;
      const buf = Buffer.from(str, 'ascii');
      this.push(buf);
    }
  }
}
```

### Implementing a Duplex Stream

A [Duplex][] stream is one that implements both [Readable][] and [Writable][],
such as a TCP socket connection.

Because JavaScript does not have support for multiple inheritance, the
`stream.Duplex` class is extended to implement a [Duplex][] stream (as opposed
to extending the `stream.Readable` *and* `stream.Writable` classes).

*Note*: The `stream.Duplex` class prototypically inherits from
`stream.Readable` and parasitically from `stream.Writable`, but `instanceof`
will work properly for both base classes due to overriding
[`Symbol.hasInstance`][] on `stream.Writable`.

Custom Duplex streams *must* call the `new stream.Duplex([options])`
constructor and implement *both* the `readable._read()` and
`writable._write()` methods.

#### new stream.Duplex(options)
<!-- YAML
changes:
  - version: v8.4.0
    pr-url: https://github.com/nodejs/node/pull/14636
    description: The `readableHighWaterMark` and `writableHighWaterMark` options
                 are supported now.
-->

* `options` {Object} Passed to both Writable and Readable
  constructors. Also has the following fields:
  * `allowHalfOpen` {boolean} Defaults to `true`. If set to `false`, then
    the stream will automatically end the writable side when the
    readable side ends.
  * `readableObjectMode` {boolean} Defaults to `false`. Sets `objectMode`
    for readable side of the stream. Has no effect if `objectMode`
    is `true`.
  * `writableObjectMode` {boolean} Defaults to `false`. Sets `objectMode`
    for writable side of the stream. Has no effect if `objectMode`
    is `true`.
  * `readableHighWaterMark` {number} Sets `highWaterMark` for the readable side
    of the stream. Has no effect if `highWaterMark` is provided.
  * `writableHighWaterMark` {number} Sets `highWaterMark` for the writable side
    of the stream. Has no effect if `highWaterMark` is provided.

For example:

```js
const { Duplex } = require('stream');

class MyDuplex extends Duplex {
  constructor(options) {
    super(options);
    // ...
  }
}
```

Or, when using pre-ES6 style constructors:

```js
const { Duplex } = require('stream');
const util = require('util');

function MyDuplex(options) {
  if (!(this instanceof MyDuplex))
    return new MyDuplex(options);
  Duplex.call(this, options);
}
util.inherits(MyDuplex, Duplex);
```

Or, using the Simplified Constructor approach:

```js
const { Duplex } = require('stream');

const myDuplex = new Duplex({
  read(size) {
    // ...
  },
  write(chunk, encoding, callback) {
    // ...
  }
});
```

#### An Example Duplex Stream

The following illustrates a simple example of a Duplex stream that wraps a
hypothetical lower-level source object to which data can be written, and
from which data can be read, albeit using an API that is not compatible with
Node.js streams.
The following illustrates a simple example of a Duplex stream that buffers
incoming written data via the [Writable][] interface that is read back out
via the [Readable][] interface.

```js
const { Duplex } = require('stream');
const kSource = Symbol('source');

class MyDuplex extends Duplex {
  constructor(source, options) {
    super(options);
    this[kSource] = source;
  }

  _write(chunk, encoding, callback) {
    // The underlying source only deals with strings
    if (Buffer.isBuffer(chunk))
      chunk = chunk.toString();
    this[kSource].writeSomeData(chunk);
    callback();
  }

  _read(size) {
    this[kSource].fetchSomeData(size, (data, encoding) => {
      this.push(Buffer.from(data, encoding));
    });
  }
}
```

The most important aspect of a Duplex stream is that the Readable and Writable
sides operate independently of one another despite co-existing within a single
object instance.

#### Object Mode Duplex Streams

For Duplex streams, `objectMode` can be set exclusively for either the Readable
or Writable side using the `readableObjectMode` and `writableObjectMode` options
respectively.

In the following example, for instance, a new Transform stream (which is a
type of [Duplex][] stream) is created that has an object mode Writable side
that accepts JavaScript numbers that are converted to hexadecimal strings on
the Readable side.

```js
const { Transform } = require('stream');

// All Transform streams are also Duplex Streams
const myTransform = new Transform({
  writableObjectMode: true,

  transform(chunk, encoding, callback) {
    // Coerce the chunk to a number if necessary
    chunk |= 0;

    // Transform the chunk into something else.
    const data = chunk.toString(16);

    // Push the data onto the readable queue.
    callback(null, '0'.repeat(data.length % 2) + data);
  }
});

myTransform.setEncoding('ascii');
myTransform.on('data', (chunk) => console.log(chunk));

myTransform.write(1);
// Prints: 01
myTransform.write(10);
// Prints: 0a
myTransform.write(100);
// Prints: 64
```

### Implementing a Transform Stream

A [Transform][] stream is a [Duplex][] stream where the output is computed
in some way from the input. Examples include [zlib][] streams or [crypto][]
streams that compress, encrypt, or decrypt data.

*Note*: There is no requirement that the output be the same size as the input,
the same number of chunks, or arrive at the same time. For example, a
Hash stream will only ever have a single chunk of output which is
provided when the input is ended. A `zlib` stream will produce output
that is either much smaller or much larger than its input.

The `stream.Transform` class is extended to implement a [Transform][] stream.

The `stream.Transform` class prototypically inherits from `stream.Duplex` and
implements its own versions of the `writable._write()` and `readable._read()`
methods. Custom Transform implementations *must* implement the
[`transform._transform()`][stream-_transform] method and *may* also implement
the [`transform._flush()`][stream-_flush] method.

*Note*: Care must be taken when using Transform streams in that data written
to the stream can cause the Writable side of the stream to become paused if
the output on the Readable side is not consumed.

#### new stream.Transform([options])

* `options` {Object} Passed to both Writable and Readable
  constructors. Also has the following fields:
  * `transform` {Function} Implementation for the
    [`stream._transform()`][stream-_transform] method.
  * `flush` {Function} Implementation for the [`stream._flush()`][stream-_flush]
    method.

For example:

```js
const { Transform } = require('stream');

class MyTransform extends Transform {
  constructor(options) {
    super(options);
    // ...
  }
}
```

Or, when using pre-ES6 style constructors:

```js
const { Transform } = require('stream');
const util = require('util');

function MyTransform(options) {
  if (!(this instanceof MyTransform))
    return new MyTransform(options);
  Transform.call(this, options);
}
util.inherits(MyTransform, Transform);
```

Or, using the Simplified Constructor approach:

```js
const { Transform } = require('stream');

const myTransform = new Transform({
  transform(chunk, encoding, callback) {
    // ...
  }
});
```

#### Events: 'finish' and 'end'

The [`'finish'`][] and [`'end'`][] events are from the `stream.Writable`
and `stream.Readable` classes, respectively. The `'finish'` event is emitted
after [`stream.end()`][stream-end] is called and all chunks have been processed
by [`stream._transform()`][stream-_transform]. The `'end'` event is emitted
after all data has been output, which occurs after the callback in
[`transform._flush()`][stream-_flush] has been called.

#### transform.\_flush(callback)

* `callback` {Function} A callback function (optionally with an error
  argument and data) to be called when remaining data has been flushed.

*Note*: This function MUST NOT be called by application code directly. It
should be implemented by child classes, and called by the internal Readable
class methods only.

In some cases, a transform operation may need to emit an additional bit of
data at the end of the stream. For example, a `zlib` compression stream will
store an amount of internal state used to optimally compress the output. When
the stream ends, however, that additional data needs to be flushed so that the
compressed data will be complete.

Custom [Transform][] implementations *may* implement the `transform._flush()`
method. This will be called when there is no more written data to be consumed,
but before the [`'end'`][] event is emitted signaling the end of the
[Readable][] stream.

Within the `transform._flush()` implementation, the `readable.push()` method
may be called zero or more times, as appropriate. The `callback` function must
be called when the flush operation is complete.

The `transform._flush()` method is prefixed with an underscore because it is
internal to the class that defines it, and should never be called directly by
user programs.

#### transform.\_transform(chunk, encoding, callback)

* `chunk` {Buffer|string|any} The chunk to be transformed. Will **always**
  be a buffer unless the `decodeStrings` option was set to `false`
  or the stream is operating in object mode.
* `encoding` {string} If the chunk is a string, then this is the
  encoding type. If chunk is a buffer, then this is the special
  value - 'buffer', ignore it in this case.
* `callback` {Function} A callback function (optionally with an error
  argument and data) to be called after the supplied `chunk` has been
  processed.

*Note*: This function MUST NOT be called by application code directly. It
should be implemented by child classes, and called by the internal Readable
class methods only.

All Transform stream implementations must provide a `_transform()`
method to accept input and produce output. The `transform._transform()`
implementation handles the bytes being written, computes an output, then passes
that output off to the readable portion using the `readable.push()` method.

The `transform.push()` method may be called zero or more times to generate
output from a single input chunk, depending on how much is to be output
as a result of the chunk.

It is possible that no output is generated from any given chunk of input data.

The `callback` function must be called only when the current chunk is completely
consumed. The first argument passed to the `callback` must be an `Error` object
if an error occurred while processing the input or `null` otherwise. If a second
argument is passed to the `callback`, it will be forwarded on to the
`readable.push()` method. In other words the following are equivalent:

```js
transform.prototype._transform = function(data, encoding, callback) {
  this.push(data);
  callback();
};

transform.prototype._transform = function(data, encoding, callback) {
  callback(null, data);
};
```

The `transform._transform()` method is prefixed with an underscore because it
is internal to the class that defines it, and should never be called directly by
user programs.

`transform._transform()` is never called in  parallel; streams implement a
queue mechanism, and to receive the next chunk, `callback` must be
called, either synchronously or asynchronously.

#### Class: stream.PassThrough

The `stream.PassThrough` class is a trivial implementation of a [Transform][]
stream that simply passes the input bytes across to the output. Its purpose is
primarily for examples and testing, but there are some use cases where
`stream.PassThrough` is useful as a building block for novel sorts of streams.

## Additional Notes

<!--type=misc-->

### Compatibility with Older Node.js Versions

<!--type=misc-->

In versions of Node.js prior to v0.10, the Readable stream interface was
simpler, but also less powerful and less useful.

* Rather than waiting for calls the [`stream.read()`][stream-read] method,
  [`'data'`][] events would begin emitting immediately. Applications that
  would need to perform some amount of work to decide how to handle data
  were required to store read data into buffers so the data would not be lost.
* The [`stream.pause()`][stream-pause] method was advisory, rather than
  guaranteed. This meant that it was still necessary to be prepared to receive
  [`'data'`][] events *even when the stream was in a paused state*.

In Node.js v0.10, the [Readable][] class was added. For backwards compatibility
with older Node.js programs, Readable streams switch into "flowing mode" when a
[`'data'`][] event handler is added, or when the
[`stream.resume()`][stream-resume] method is called. The effect is that, even
when not using the new [`stream.read()`][stream-read] method and
[`'readable'`][] event, it is no longer necessary to worry about losing
[`'data'`][] chunks.

While most applications will continue to function normally, this introduces an
edge case in the following conditions:

* No [`'data'`][] event listener is added.
* The [`stream.resume()`][stream-resume] method is never called.
* The stream is not piped to any writable destination.

For example, consider the following code:

```js
// WARNING!  BROKEN!
net.createServer((socket) => {

  // we add an 'end' method, but never consume the data
  socket.on('end', () => {
    // It will never get here.
    socket.end('The message was received but was not processed.\n');
  });

}).listen(1337);
```

In versions of Node.js prior to v0.10, the incoming message data would be
simply discarded. However, in Node.js v0.10 and beyond, the socket remains
paused forever.

The workaround in this situation is to call the
[`stream.resume()`][stream-resume] method to begin the flow of data:

```js
// Workaround
net.createServer((socket) => {

  socket.on('end', () => {
    socket.end('The message was received but was not processed.\n');
  });

  // start the flow of data, discarding it.
  socket.resume();

}).listen(1337);
```

In addition to new Readable streams switching into flowing mode,
pre-v0.10 style streams can be wrapped in a Readable class using the
[`readable.wrap()`][`stream.wrap()`] method.


### `readable.read(0)`

There are some cases where it is necessary to trigger a refresh of the
underlying readable stream mechanisms, without actually consuming any
data. In such cases, it is possible to call `readable.read(0)`, which will
always return `null`.

If the internal read buffer is below the `highWaterMark`, and the
stream is not currently reading, then calling `stream.read(0)` will trigger
a low-level [`stream._read()`][stream-_read] call.

While most applications will almost never need to do this, there are
situations within Node.js where this is done, particularly in the
Readable stream class internals.

### `readable.push('')`

Use of `readable.push('')` is not recommended.

Pushing a zero-byte string, `Buffer` or `Uint8Array` to a stream that is not in
object mode has an interesting side effect. Because it *is* a call to
[`readable.push()`][stream-push], the call will end the reading process.
However, because the argument is an empty string, no data is added to the
readable buffer so there is nothing for a user to consume.

[`'data'`]: #stream_event_data
[`'drain'`]: #stream_event_drain
[`'end'`]: #stream_event_end
[`'finish'`]: #stream_event_finish
[`'readable'`]: #stream_event_readable
[`EventEmitter`]: events.html#events_class_eventemitter
[`Symbol.hasInstance`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Symbol/hasInstance
[`fs.createReadStream()`]: fs.html#fs_fs_createreadstream_path_options
[`fs.createWriteStream()`]: fs.html#fs_fs_createwritestream_path_options
[`net.Socket`]: net.html#net_class_net_socket
[`process.stderr`]: process.html#process_process_stderr
[`process.stdin`]: process.html#process_process_stdin
[`process.stdout`]: process.html#process_process_stdout
[`stream.cork()`]: #stream_writable_cork
[`stream.pipe()`]: #stream_readable_pipe_destination_options
[`stream.uncork()`]: #stream_writable_uncork
[`stream.unpipe()`]: #stream_readable_unpipe_destination
[`stream.wrap()`]: #stream_readable_wrap_stream
[`writable.cork()`]: #stream_writable_cork
[`writable.uncork()`]: #stream_writable_uncork
[`zlib.createDeflate()`]: zlib.html#zlib_zlib_createdeflate_options
[API for Stream Consumers]: #stream_api_for_stream_consumers
[API for Stream Implementers]: #stream_api_for_stream_implementers
[Compatibility]: #stream_compatibility_with_older_node_js_versions
[Duplex]: #stream_class_stream_duplex
[HTTP requests, on the client]: http.html#http_class_http_clientrequest
[HTTP responses, on the server]: http.html#http_class_http_serverresponse
[Readable]: #stream_class_stream_readable
[TCP sockets]: net.html#net_class_net_socket
[Transform]: #stream_class_stream_transform
[Writable]: #stream_class_stream_writable
[child process stdin]: child_process.html#child_process_child_stdin
[child process stdout and stderr]: child_process.html#child_process_child_stdout
[crypto]: crypto.html
[fs read streams]: fs.html#fs_class_fs_readstream
[fs write streams]: fs.html#fs_class_fs_writestream
[http-incoming-message]: http.html#http_class_http_incomingmessage
[zlib]: zlib.html
[stream-_flush]: #stream_transform_flush_callback
[stream-_read]: #stream_readable_read_size_1
[stream-_transform]: #stream_transform_transform_chunk_encoding_callback
[stream-_write]: #stream_writable_write_chunk_encoding_callback_1
[stream-_writev]: #stream_writable_writev_chunks_callback
[stream-_final]: #stream_writable_final_callback
[stream-end]: #stream_writable_end_chunk_encoding_callback
[stream-pause]: #stream_readable_pause
[stream-push]: #stream_readable_push_chunk_encoding
[stream-read]: #stream_readable_read_size
[stream-resume]: #stream_readable_resume
[stream-write]: #stream_writable_write_chunk_encoding_callback
[readable-_destroy]: #stream_readable_destroy_err_callback
[writable-_destroy]: #stream_writable_destroy_err_callback
