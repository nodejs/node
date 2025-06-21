// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const { StreamWrap } = require('internal/js_stream_socket');
const { Duplex } = require('stream');
const { internalBinding } = require('internal/test/binding');
const { ShutdownWrap } = internalBinding('stream_wrap');

// This test makes sure that when a wrapped stream is waiting for
// a "drain" event to `doShutdown`, the instance will work correctly when a
// "drain" event emitted.
{
  let resolve = null;

  class TestDuplex extends Duplex {
    _write(chunk, encoding, callback) {
      // We will resolve the write later.
      resolve = () => {
        callback();
      };
    }

    _read() {}
  }

  const testDuplex = new TestDuplex();
  const socket = new StreamWrap(testDuplex);

  socket.write(
    // Make the buffer long enough so that the `Writable` will emit "drain".
    Buffer.allocUnsafe(socket.writableHighWaterMark * 2),
    common.mustCall()
  );

  // Make sure that the 'drain' events will be emitted.
  testDuplex.on('drain', common.mustCall(() => {
    console.log('testDuplex drain');
  }));

  assert.strictEqual(typeof resolve, 'function');

  const req = new ShutdownWrap();
  req.oncomplete = common.mustCall();
  req.handle = socket._handle;
  // Should not throw.
  socket._handle.shutdown(req);

  resolve();
}
