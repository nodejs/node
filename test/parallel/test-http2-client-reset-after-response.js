'use strict';

// RFC 9113 section 8.1 allows a server to send RST_STREAM(NO_ERROR) after a
// complete response while the request is still open, and clients must not
// discard the response because of it. This is how servers send clients a
// response that cancels a request (e.g. politely rejecting an upload).
// The response should still stay fully readable and end cleanly, while the
// rest of the request is quietly discarded.
// Refs: https://github.com/nodejs/node/issues/66693

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('http2');
const { once } = require('events');

const body = 'upload rejected';

const server = http2.createServer();
server.on('stream', common.mustCall((stream) => {
  stream.respond({ ':status': 413 });
  stream.once('data', common.mustCall(() => {
    stream.end(body);
    stream.close(http2.constants.NGHTTP2_NO_ERROR);
  }));
}));

server.listen(0, common.mustCall(async () => {
  const session = http2.connect(`http://localhost:${server.address().port}`);
  const request = session.request({ ':method': 'POST' });
  request.on('error', common.mustNotCall());
  const [headers] = await once(request, 'response');
  assert.strictEqual(headers[':status'], 413);

  // Keep uploading without reading the response. The reset arrives partway
  // through, and neither the write in flight then nor later ones may stall.
  const chunk = Buffer.alloc(16 * 1024);
  for (let i = 0; i < 20; i++) {
    await new Promise((resolve) => {
      request.write(chunk, common.mustSucceed(resolve));
    });
  }
  assert.strictEqual(request.closed, true);
  assert.strictEqual(request.rstCode, http2.constants.NGHTTP2_NO_ERROR);
  assert.strictEqual(request.destroyed, false);

  request.end();
  await once(request, 'finish');

  let received = '';
  request.setEncoding('utf8');
  request.on('data', (chunk) => { received += chunk; });
  request.on('end', common.mustCall());
  await once(request, 'close');
  assert.strictEqual(received, body);

  session.close();
  server.close();
}));
