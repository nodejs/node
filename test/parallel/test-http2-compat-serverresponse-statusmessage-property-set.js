// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

// Http2ServerResponse.statusMessage should warn

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
    response.on('finish', common.mustCall(function() {
      response.statusMessage = 'test';
      response.statusMessage = 'test'; // only warn once
      assert.strictEqual(response.statusMessage, ''); // no change
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
    }, 1));
    request.on('end', common.mustCall(function() {
      client.destroy();
    }));
    request.end();
    request.resume();
  }));
}));
