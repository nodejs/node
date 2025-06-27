'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

// Http2ServerResponse.writeHead should accept an optional status message

const unsupportedWarned = common.mustCall(1);
process.on('warning', ({ name, message }) => {
  const expectedMessage =
    'Status message is not supported by HTTP/2 (RFC7540 8.1.2.4)';
  if (name === 'UnsupportedWarning' && message === expectedMessage)
    unsupportedWarned();
});

const server = h2.createServer();
server.listen(0, common.mustCall(function() {
  const port = server.address().port;
  server.once('request', common.mustCall(function(request, response) {
    const statusCode = 200;
    const statusMessage = 'OK';
    const headers = { 'foo-bar': 'abc123' };
    response.writeHead(statusCode, statusMessage, headers);

    response.on('finish', common.mustCall(function() {
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
      assert.strictEqual(headers[':status'], 200);
      assert.strictEqual(headers['foo-bar'], 'abc123');
    }, 1));
    request.on('end', common.mustCall(function() {
      client.close();
    }));
    request.end();
    request.resume();
  }));
}));
