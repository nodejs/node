// Flags: --expose-http2
'use strict';

const common = require('../common');
const assert = require('assert');
const h2 = require('http2');

// Http2ServerResponse.writeHead should override previous headers

const server = h2.createServer();
server.listen(0, common.mustCall(function() {
  const port = server.address().port;
  server.once('request', common.mustCall(function(request, response) {
    response.setHeader('foo-bar', 'def456');
    response.writeHead(500);
    response.writeHead(418, {'foo-bar': 'abc123'}); // Override

    response.on('finish', common.mustCall(function() {
      assert.doesNotThrow(() => { response.writeHead(300); });
      server.close();
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
      client.destroy();
    }));
    request.end();
    request.resume();
  }));
}));
