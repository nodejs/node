// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const server = http2.createServer();
server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  server.once('request', common.mustCall((request, response) => {
    response.addTrailers({
      ABC: 123
    });
    response.end('hello');
  }));

  const url = `http://localhost:${port}`;
  const client = http2.connect(url, common.mustCall(() => {
    const request = client.request();
    request.on('trailers', common.mustCall((headers) => {
      assert.strictEqual(headers.abc, '123');
    }));
    request.resume();
    request.on('end', common.mustCall(() => {
      client.destroy();
      server.close();
    }));
  }));
}));
