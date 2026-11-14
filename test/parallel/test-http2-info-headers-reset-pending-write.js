// Flags: --expose-gc
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('http2');
const { duplexPair } = require('stream');
const { setImmediate } = require('timers/promises');

// The stream can be reset and garbage collected while the write that carries
// its informational headers is still pending. The callback must not be called
// before that write has finished, and completing it must not crash.

const { NGHTTP2_CANCEL } = http2.constants;
const kHeadersFrameType = 1;

function hasHeadersFrame(buf) {
  for (let offset = 0; offset + 9 <= buf.length;) {
    if (buf[offset + 3] === kHeadersFrameType)
      return true;
    offset += 9 + buf.readUIntBE(offset, 3);
  }
  return false;
}

(async () => {
  const [clientSide, serverSide] = duplexPair();
  let pendingWrite;
  const originalWrite = serverSide._write;
  serverSide._write = common.mustCallAtLeast(function(buf, enc, cb) {
    // Deliver the data, but do not complete the write that carries the
    // informational headers.
    const hold = hasHeadersFrame(buf);
    originalWrite.call(this, buf, enc, common.mustCall(() => {
      if (hold) {
        assert.strictEqual(pendingWrite, undefined);
        pendingWrite = cb;
      } else {
        cb();
      }
    }));
  });

  const server = http2.createServer();
  let called = false;
  let streamClosed;
  const closed = new Promise((resolve) => { streamClosed = resolve; });
  server.on('stream', common.mustCall((stream) => {
    stream.on('close', common.mustCall(streamClosed));
    stream.additionalHeaders({ ':status': 103 }, common.mustCall((err) => {
      assert.strictEqual(err, null);
      called = true;
    }));
  }));
  server.emit('connection', serverSide);

  const client = http2.connect('http://localhost', {
    createConnection: () => clientSide,
  });
  const req = client.request();
  req.on('headers', common.mustCall(() => req.close(NGHTTP2_CANCEL)));
  req.resume();

  await closed;
  // Let the deferred native stream cleanup run, then collect the wrapper
  // while the write is still awaiting completion.
  for (let i = 0; i < 10; i++) {
    await setImmediate();
    global.gc();
  }
  assert.strictEqual(typeof pendingWrite, 'function');
  assert.strictEqual(called, false);
  pendingWrite();
  await setImmediate();
  assert.strictEqual(called, true);

  client.destroy();
  server.close();
})().then(common.mustCall());
