'use strict';

// An HTTP/2 stream that closed normally can still hold its whole response in
// the readable buffer. Starting async iteration at that point must yield the
// buffered body rather than reporting ERR_STREAM_PREMATURE_CLOSE.
// Refs: https://github.com/nodejs/node/issues/65677

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('http2');
const { once } = require('events');
const { setImmediate: setImmediateAsync } = require('timers/promises');

const body = 'complete response';

const server = http2.createServer();
server.on('stream', (stream) => {
  stream.resume();
  stream.on('end', () => {
    stream.respond({ ':status': 200 }, { waitForTrailers: true });
    stream.on('wantTrailers', () => stream.sendTrailers({ 'grpc-status': '0' }));
    stream.end(body);
  });
});

server.listen(0, '127.0.0.1');

(async () => {
  await once(server, 'listening');
  const session = http2.connect(`http://127.0.0.1:${server.address().port}`);

  try {
    await once(session, 'connect');
    const request = session.request({ ':method': 'POST' });
    const response = once(request, 'response');
    request.end('request');
    await response;

    // Hold off consuming the response until the stream reports a normal close.
    // The body is buffered at this point and the stream is not destroyed.
    while (!request.closed)
      await setImmediateAsync();

    assert.strictEqual(request.rstCode, 0);
    assert.strictEqual(request.destroyed, false);
    assert.strictEqual(request.readableLength, Buffer.byteLength(body));

    let received = '';
    for await (const chunk of request)
      received += chunk;

    assert.strictEqual(received, body);
  } finally {
    session.destroy();
    await once(session, 'close');
    server.close();
  }
})().then(common.mustCall());
