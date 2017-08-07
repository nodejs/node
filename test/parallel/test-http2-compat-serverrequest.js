// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');
const net = require('net');

// Http2ServerRequest should expose convenience properties

const server = h2.createServer();
server.listen(0, common.mustCall(function() {
  const port = server.address().port;
  server.once('request', common.mustCall(function(request, response) {
    const expected = {
      statusCode: null,
      version: '2.0',
      httpVersionMajor: 2,
      httpVersionMinor: 0
    };

    assert.strictEqual(request.statusCode, expected.statusCode);
    assert.strictEqual(request.httpVersion, expected.version);
    assert.strictEqual(request.httpVersionMajor, expected.httpVersionMajor);
    assert.strictEqual(request.httpVersionMinor, expected.httpVersionMinor);

    assert.ok(request.socket instanceof net.Socket);
    assert.ok(request.connection instanceof net.Socket);
    assert.strictEqual(request.socket, request.connection);

    response.on('finish', common.mustCall(function() {
      server.close();
    }));
    response.end();
  }));

  const url = `http://localhost:${port}`;
  const client = h2.connect(url, common.mustCall(function() {
    const headers = {
      ':path': '/foobar',
      ':method': 'GET',
      ':scheme': 'http',
      ':authority': `localhost:${port}`
    };
    const request = client.request(headers);
    request.on('end', common.mustCall(function() {
      client.destroy();
    }));
    request.end();
    request.resume();
  }));
}));
