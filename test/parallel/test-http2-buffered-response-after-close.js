'use strict';

// A client stream that closed normally can still hold its whole response in
// the readable buffer. Consuming it from then on must succeed rather than
// report ERR_STREAM_PREMATURE_CLOSE.
// Refs: https://github.com/nodejs/node/issues/65677

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('http2');
const { once } = require('events');
const { Writable } = require('stream');
const { finished, pipeline } = require('stream/promises');
const { text } = require('stream/consumers');
const { setImmediate: setImmediateAsync } = require('timers/promises');

const body = 'complete response';

const consumers = {
  async asyncIterator(request) {
    let received = '';
    for await (const chunk of request)
      received += chunk;
    return received;
  },
  async finishedThenData(request) {
    let received = '';
    const done = finished(request, { writable: false });
    request.on('data', (chunk) => { received += chunk; });
    await done;
    return received;
  },
  async pipeline(request) {
    let received = '';
    await pipeline(request, new Writable({
      write(chunk, encoding, callback) {
        received += chunk;
        callback();
      },
    }));
    return received;
  },
  text,
};

const server = http2.createServer();
server.on('stream', (stream) => {
  stream.resume();
  stream.on('end', () => {
    stream.respond({ ':status': 200 }, { waitForTrailers: true });
    stream.on('wantTrailers', () => stream.sendTrailers({ 'grpc-status': '0' }));
    stream.end(body);
  });
});

server.listen(0, common.mustCall(async () => {
  const session = http2.connect(`http://localhost:${server.address().port}`);

  for (const [name, consume] of Object.entries(consumers)) {
    const request = session.request({ ':method': 'POST' });
    request.end('request');
    await once(request, 'response');

    // Attempt to read the response _after_ the server has cleanly closed the stream:
    while (!request.closed)
      await setImmediateAsync();

    assert.strictEqual(request.rstCode, http2.constants.NGHTTP2_NO_ERROR);
    assert.strictEqual(request.destroyed, false);
    assert.strictEqual(request.readableLength, Buffer.byteLength(body));
    assert.strictEqual(await consume(request), body, name);
  }

  session.close();
  server.close();
}));
