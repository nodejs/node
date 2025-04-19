'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

const server = h2.createServer();
let session;

server.on('session', common.mustCall(function(s) {
  session = s;
  session.on('close', common.mustCall(function() {
    server.close();
  }));
}));

server.listen(0, common.mustCall(function() {
  const port = server.address().port;

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
      client.close();
    }));
    request.end();
    request.resume();
  }));
  client.on('goaway', common.mustCallAtLeast(1));
}));

server.once('request', common.mustCall(function(request, response) {
  response.on('finish', common.mustCall(function() {
    session.close();
  }));
  response.end();
}));
