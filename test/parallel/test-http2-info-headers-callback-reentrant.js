'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('http2');
const { duplexPair } = require('stream');
const { setImmediate } = require('timers/promises');

// Informational headers that are sent from the completion callback of another
// write belong to the next write to the underlying stream. Their callback must
// not be called before that write has finished.

(async () => {
  const [clientSide, serverSide] = duplexPair();
  let hold = false;
  let pendingWrite;
  const originalWrite = serverSide._write;
  serverSide._write = common.mustCallAtLeast(function(buf, enc, cb) {
    if (hold) {
      // Deliver the data, but do not complete the write.
      assert.strictEqual(pendingWrite, undefined);
      originalWrite.call(this, buf, enc, common.mustCall(() => {
        pendingWrite = cb;
      }));
      return;
    }
    originalWrite.call(this, buf, enc, cb);
  });

  const server = http2.createServer();
  let called = false;
  let first;
  server.on('stream', common.mustCall((stream) => {
    if (first === undefined) {
      first = stream;
      return;
    }
    first.respond();
    first.write('data', common.mustCall(() => {
      // This runs while the write that carried the DATA frame is being
      // completed. Closing the first stream starts the next write, which
      // carries the informational headers of the second stream.
      hold = true;
      stream.additionalHeaders({ ':status': 103 }, common.mustCall((err) => {
        assert.strictEqual(err, null);
        called = true;
      }));
      first.close();
    }));
  }, 2));
  server.emit('connection', serverSide);

  const client = http2.connect('http://localhost', {
    createConnection: () => clientSide,
  });
  const req1 = client.request();
  req1.on('error', () => {});
  req1.resume();
  const req2 = client.request();
  req2.resume();
  await new Promise((resolve) => req2.on('headers', common.mustCall(resolve)));

  assert.strictEqual(typeof pendingWrite, 'function');
  assert.strictEqual(called, false);
  hold = false;
  pendingWrite();
  await setImmediate();
  assert.strictEqual(called, true);

  client.destroy();
  server.close();
})().then(common.mustCall());
