'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

// A clean peer reset must retain 26.x's buffered reads and readable 'end',
// rather than introducing the reset errors and immediate destroy from main.
const body = 'buffered response';
const server = http2.createServer();
server.on('stream', common.mustCall((stream) => {
  stream.on('error', common.mustNotCall());
  stream.resume();
  stream.respond();
  stream.write(body, common.mustCall(() => stream.close()));
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  const request = client.request({ ':method': 'POST' });
  request.write('request body');
  request.pause();
  request.read(0);
  request.on('response', common.mustCall());
  request.on('error', common.mustNotCall());
  request.on('aborted', common.mustCall(() => {
    setImmediate(common.mustCall(() => {
      assert.strictEqual(request.destroyed, false);
      assert.strictEqual(request.readableLength, Buffer.byteLength(body));
      let received = '';
      request.on('data', (chunk) => { received += chunk; });
      request.on('end', common.mustCall(() => {
        assert.strictEqual(received, body);
      }));
      request.resume();
    }));
  }));
  request.on('close', common.mustCall(() => {
    assert.strictEqual(request.readableEnded, true);
    assert.strictEqual(request.rstCode, http2.constants.NGHTTP2_NO_ERROR);
    client.close();
    server.close();
  }));
}));
