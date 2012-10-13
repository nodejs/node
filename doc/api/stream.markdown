# Stream

    Stability: 2 - Unstable

A stream is an abstract interface implemented by various objects in
Node.  For example a request to an HTTP server is a stream, as is
stdout. Streams are readable, writable, or both. All streams are
instances of [EventEmitter][]

You can load up the Stream base class by doing `require('stream')`.

## Readable Stream

<!--type=class-->

A `Readable Stream` has the following methods, members, and events.

### Event: 'data'

`function (data) { }`

The `'data'` event emits either a `Buffer` (by default) or a string if
`setEncoding()` was used.

Note that the __data will be lost__ if there is no listener when a
`Readable Stream` emits a `'data'` event.

### Event: 'end'

`function () { }`

Emitted when the stream has received an EOF (FIN in TCP terminology).
Indicates that no more `'data'` events will happen. If the stream is
also writable, it may be possible to continue writing.

### Event: 'error'

`function (exception) { }`

Emitted if there was an error receiving data.

### Event: 'close'

`function () { }`

Emitted when the underlying resource (for example, the backing file
descriptor) has been closed. Not all streams will emit this.

### stream.readable

A boolean that is `true` by default, but turns `false` after an
`'error'` occurred, the stream came to an `'end'`, or `destroy()` was
called.

### stream.setEncoding([encoding])

Makes the `'data'` event emit a string instead of a `Buffer`. `encoding`
can be `'utf8'`, `'utf16le'` (`'ucs2'`), `'ascii'`, or `'hex'`. Defaults
to `'utf8'`.

### stream.pause()

Issues an advisory signal to the underlying communication layer,
requesting that no further data be sent until `resume()` is called.

Note that, due to the advisory nature, certain streams will not be
paused immediately, and so `'data'` events may be emitted for some
indeterminate period of time even after `pause()` is called. You may
wish to buffer such `'data'` events.

### stream.resume()

Resumes the incoming `'data'` events after a `pause()`.

### stream.destroy()

Closes the underlying file descriptor. Stream is no longer `writable`
nor `readable`.  The stream will not emit any more 'data', or 'end'
events. Any queued write data will not be sent.  The stream should emit
'close' event once its resources have been disposed of.


### stream.pipe(destination, [options])

This is a `Stream.prototype` method available on all `Stream`s.

Connects this read stream to `destination` WriteStream. Incoming data on
this stream gets written to `destination`. The destination and source
streams are kept in sync by pausing and resuming as necessary.

This function returns the `destination` stream.

Emulating the Unix `cat` command:

    process.stdin.resume(); process.stdin.pipe(process.stdout);


By default `end()` is called on the destination when the source stream
emits `end`, so that `destination` is no longer writable. Pass `{ end:
false }` as `options` to keep the destination stream open.

This keeps `process.stdout` open so that "Goodbye" can be written at the
end.

    process.stdin.resume();

    process.stdin.pipe(process.stdout, { end: false });

    process.stdin.on("end", function() {
    process.stdout.write("Goodbye\n"); });


## Writable Stream

<!--type=class-->

A `Writable Stream` has the following methods, members, and events.

### Event: 'drain'

`function () { }`

Emitted when the stream's write queue empties and it's safe to write without
buffering again. Listen for it when `stream.write()` returns `false`.

The `'drain'` event can happen at *any* time, regardless of whether or not
`stream.write()` has previously returned `false`. To avoid receiving unwanted
`'drain'` events, listen using `stream.once()`.

### Event: 'error'

`function (exception) { }`

Emitted on error with the exception `exception`.

### Event: 'close'

`function () { }`

Emitted when the underlying file descriptor has been closed.

### Event: 'pipe'

`function (src) { }`

Emitted when the stream is passed to a readable stream's pipe method.

### stream.writable

A boolean that is `true` by default, but turns `false` after an
`'error'` occurred or `end()` / `destroy()` was called.

### stream.write(string, [encoding])

Writes `string` with the given `encoding` to the stream.  Returns `true`
if the string has been flushed to the kernel buffer.  Returns `false` to
indicate that the kernel buffer is full, and the data will be sent out
in the future. The `'drain'` event will indicate when the kernel buffer
is empty again. The `encoding` defaults to `'utf8'`.

### stream.write(buffer)

Same as the above except with a raw buffer.

### stream.end()

Terminates the stream with EOF or FIN.  This call will allow queued
write data to be sent before closing the stream.

### stream.end(string, encoding)

Sends `string` with the given `encoding` and terminates the stream with
EOF or FIN. This is useful to reduce the number of packets sent.

### stream.end(buffer)

Same as above but with a `buffer`.

### stream.destroy()

Closes the underlying file descriptor. Stream is no longer `writable`
nor `readable`.  The stream will not emit any more 'data', or 'end'
events. Any queued write data will not be sent.  The stream should emit
'close' event once its resources have been disposed of.

### stream.destroySoon()

After the write queue is drained, close the file descriptor.
`destroySoon()` can still destroy straight away, as long as there is no
data left in the queue for writes.

[EventEmitter]: events.html#events_class_events_eventemitter
