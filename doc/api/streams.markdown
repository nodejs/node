## Streams

A stream is an abstract interface implemented by various objects in Node.
For example a request to an HTTP server is a stream, as is stdout. Streams
are readable, writable, or both. All streams are instances of `EventEmitter`.

## Readable Stream

A `Readable Stream` has the following methods, members, and events.

### Event: 'data'

`function (data) { }`

The `'data'` event emits either a `Buffer` (by default) or a string if
`setEncoding()` was used.

### Event: 'end'

`function () { }`

Emitted when the stream has received an EOF (FIN in TCP terminology).
Indicates that no more `'data'` events will happen. If the stream is also
writable, it may be possible to continue writing.

### Event: 'error'

`function (exception) { }`

Emitted if there was an error receiving data.

### Event: 'close'

`function () { }`

Emitted when the underlying file descriptor has been closed. Not all streams
will emit this.  (For example, an incoming HTTP request will not emit
`'close'`.)

### Event: 'fd'

`function (fd) { }`

Emitted when a file descriptor is received on the stream. Only UNIX streams
support this functionality; all others will simply never emit this event.

### stream.readable

A boolean that is `true` by default, but turns `false` after an `'error'`
occurred, the stream came to an `'end'`, or `destroy()` was called.

### stream.setEncoding(encoding)
Makes the data event emit a string instead of a `Buffer`. `encoding` can be
`'utf8'`, `'ascii'`, or `'base64'`.

### stream.pause()

Pauses the incoming `'data'` events.

### stream.resume()

Resumes the incoming `'data'` events after a `pause()`.

### stream.destroy()

Closes the underlying file descriptor. Stream will not emit any more events.


### stream.destroySoon()

After the write queue is drained, close the file descriptor.

### stream.pipe(destination, [options])

This is a `Stream.prototype` method available on all `Stream`s.

Connects this read stream to `destination` WriteStream. Incoming
data on this stream gets written to `destination`. The destination and source
streams are kept in sync by pausing and resuming as necessary.

Emulating the Unix `cat` command:

    process.stdin.resume();
    process.stdin.pipe(process.stdout);


By default `end()` is called on the destination when the source stream emits
`end`, so that `destination` is no longer writable. Pass `{ end: false }` as
`options` to keep the destination stream open.

This keeps `process.stdout` open so that "Goodbye" can be written at the end.

    process.stdin.resume();

    process.stdin.pipe(process.stdout, { end: false });

    process.stdin.on("end", function() {
      process.stdout.write("Goodbye\n");
    });

NOTE: If the source stream does not support `pause()` and `resume()`, this function
adds simple definitions which simply emit `'pause'` and `'resume'` events on
the source stream.

## Writable Stream

A `Writable Stream` has the following methods, members, and events.

### Event: 'drain'

`function () { }`

Emitted after a `write()` method was called that returned `false` to
indicate that it is safe to write again.

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

A boolean that is `true` by default, but turns `false` after an `'error'`
occurred or `end()` / `destroy()` was called.

### stream.write(string, encoding='utf8', [fd])

Writes `string` with the given `encoding` to the stream.  Returns `true` if
the string has been flushed to the kernel buffer.  Returns `false` to
indicate that the kernel buffer is full, and the data will be sent out in
the future. The `'drain'` event will indicate when the kernel buffer is
empty again. The `encoding` defaults to `'utf8'`.

If the optional `fd` parameter is specified, it is interpreted as an integral
file descriptor to be sent over the stream. This is only supported for UNIX
streams, and is silently ignored otherwise. When writing a file descriptor in
this manner, closing the descriptor before the stream drains risks sending an
invalid (closed) FD.

### stream.write(buffer)

Same as the above except with a raw buffer.

### stream.end()

Terminates the stream with EOF or FIN.

### stream.end(string, encoding)

Sends `string` with the given `encoding` and terminates the stream with EOF
or FIN. This is useful to reduce the number of packets sent.

### stream.end(buffer)

Same as above but with a `buffer`.

### stream.destroy()

Closes the underlying file descriptor. Stream will not emit any more events.

### stream.destroySoon()

After the write queue is drained, close the file descriptor. `destroySoon()`
can still destroy straight away, as long as there is no data left in the queue
for writes.
