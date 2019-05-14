'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

// Http2ServerResponse.writeHead should support nested arrays

const server = h2.createServer();
server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  server.once('request', common.mustCall((request, response) => {
    const returnVal = response.writeHead(200, [
      ['foo', 'bar'],
      ['ABC', 123]
    ]);
    assert.strictEqual(returnVal, response);
    response.end(common.mustCall(() => { server.close(); }));
  }));

  const url = `http://localhost:${port}`;
  const client = h2.connect(url, common.mustCall(() => {
    const headers = {
      ':path': '/',
      ':method': 'GET',
      ':scheme': 'http',
      ':authority': `localhost:${port}`
    };
    const request = client.request(headers);
    request.on('response', common.mustCall((headers) => {
      assert.strictEqual(headers.foo, 'bar');
      assert.strictEqual(headers.abc, '123');
      assert.strictEqual(headers[':status'], 200);
    }, 1));
    request.on('end', common.mustCall(() => {
      client.close();
    }));
    request.end();
    request.resume();
  }));
}));
