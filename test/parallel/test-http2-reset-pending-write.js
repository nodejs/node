'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

// A peer reset may leave a write unfinished. Preserve the clean readable
// end in 26.x, but do not wait for writable 'finish' to destroy the stream.
for (const compat of [false, true]) {
  const server = http2.createServer();
  let client;
  let request;

  function onRequest(readable, writable, stream) {
    readable.on('error', common.mustNotCall());
    if (writable !== readable)
      writable.on('error', common.mustNotCall());
    stream.on('error', common.mustNotCall());
    readable.resume();
    readable.on('end', common.mustCall());
    stream.on('aborted', common.mustCall());
    stream.on('finish', common.mustNotCall());
    if (compat) {
      // The compat response retains its existing finish-on-close behavior.
      writable.on('finish', common.mustCall());
    }
    stream.on('close', common.mustCall(() => {
      assert.strictEqual(stream.rstCode, http2.constants.NGHTTP2_NO_ERROR);
      assert.strictEqual(stream.readableEnded, true);
      assert.strictEqual(stream.writableFinished, false);
      assert.strictEqual(stream.destroyed, true);
      client.close();
      server.close();
    }));

    writable.write('first response', common.mustCall(() => {
      // Model a write whose callback cannot finish after the peer resets.
      // Start the reset only once the write is pending.
      stream._write = common.mustCall(() => {
        setImmediate(() => request.destroy());
      });
      writable.write('pending response');
    }));
  }

  if (compat) {
    server.on('request', common.mustCall((req, res) => {
      onRequest(req, res, req.stream);
    }));
  } else {
    server.on('stream', common.mustCall((stream) => {
      stream.respond();
      onRequest(stream, stream, stream);
    }));
  }

  server.listen(0, common.mustCall(() => {
    client = http2.connect(`http://localhost:${server.address().port}`);
    request = client.request({ ':method': 'POST' });
    request.on('close', common.mustCall());
    request.on('error', common.mustNotCall());
    request.resume();
    request.write('request body');
  }));
}
