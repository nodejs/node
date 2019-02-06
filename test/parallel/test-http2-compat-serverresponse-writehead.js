'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

// Http2ServerResponse.writeHead should override previous headers

const server = h2.createServer();
server.listen(0, common.mustCall(function() {
  const port = server.address().port;
  server.once('request', common.mustCall(function(request, response) {
    response.setHeader('foo-bar', 'def456');

    // Override
    const returnVal = response.writeHead(418, { 'foo-bar': 'abc123' });

    assert.strictEqual(returnVal, response);

    common.expectsError(() => { response.writeHead(300); }, {
      code: 'ERR_HTTP2_HEADERS_SENT'
    });

    response.on('finish', common.mustCall(function() {
      server.close();
      process.nextTick(common.mustCall(() => {
        common.expectsError(() => { response.writeHead(300); }, {
          code: 'ERR_HTTP2_INVALID_STREAM'
        });
      }));
    }));
    response.end();
  }));

  const url = `http://localhost:${port}`;
  const client = h2.connect(url, common.mustCall(function() {
    const headers = {
      ':path': '/',
      ':method': 'GET',
      ':scheme': 'http',
      ':authority': `localhost:${port}`
    };
    const request = client.request(headers);
    request.on('response', common.mustCall(function(headers) {
      assert.strictEqual(headers['foo-bar'], 'abc123');
      assert.strictEqual(headers[':status'], 418);
    }, 1));
    request.on('end', common.mustCall(function() {
      client.close();
    }));
    request.end();
    request.resume();
  }));
}));
