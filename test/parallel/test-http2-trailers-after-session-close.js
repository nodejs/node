'use strict';

// Fixes: https://github.com/nodejs/node/issues/42713
const common = require('../common');
if (!common.hasCrypto) {
  common.skip('missing crypto');
}
const assert = require('assert');
const http2 = require('http2');

const {
  HTTP2_HEADER_PATH,
  HTTP2_HEADER_STATUS,
  HTTP2_HEADER_METHOD,
} = http2.constants;

const server = http2.createServer();
server.on('stream', common.mustCall((stream) => {
  server.close();
  stream.session.close();
  stream.on('wantTrailers', common.mustCall(() => {
    stream.sendTrailers({ xyz: 'abc' });
  }));

  stream.respond({ [HTTP2_HEADER_STATUS]: 200 }, { waitForTrailers: true });
  stream.write('some data');
  stream.end();
}));

server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  const client = http2.connect(`http://localhost:${port}`);
  client.socket.on('close', common.mustCall());
  const req = client.request({
    [HTTP2_HEADER_PATH]: '/',
    [HTTP2_HEADER_METHOD]: 'POST',
  });
  req.end();
  req.on('response', common.mustCall());
  let data = '';
  req.on('data', (chunk) => {
    data += chunk;
  });
  req.on('end', common.mustCall(() => {
    assert.strictEqual(data, 'some data');
  }));
  req.on('trailers', common.mustCall((headers) => {
    assert.strictEqual(headers.xyz, 'abc');
  }));
  req.on('close', common.mustCall());
}));
