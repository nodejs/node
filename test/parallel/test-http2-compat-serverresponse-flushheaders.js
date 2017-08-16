// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

// Http2ServerResponse.flushHeaders

let serverResponse;

const server = h2.createServer();
server.listen(0, common.mustCall(function() {
  const port = server.address().port;
  server.once('request', common.mustCall(function(request, response) {
    response.flushHeaders();
    response.flushHeaders(); // Idempotent
    common.expectsError(() => {
      response.writeHead(400, { 'foo-bar': 'abc123' });
    }, {
      code: 'ERR_HTTP2_INFO_HEADERS_AFTER_RESPOND'
    });

    response.on('finish', common.mustCall(function() {
      server.close();
    }));
    serverResponse = response;
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
    request.on('response', common.mustCall(function(headers, flags) {
      assert.strictEqual(headers['foo-bar'], undefined);
      assert.strictEqual(headers[':status'], 200);
      serverResponse.end();
    }, 1));
    request.on('end', common.mustCall(function() {
      client.destroy();
    }));
    request.end();
    request.resume();
  }));
}));
