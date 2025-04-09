'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

const server = h2.createServer();
let response;

server.on('session', common.mustCall(function(session) {
  session.on('stream', common.mustCall(function (stream) {
    response.end();
    session.close();
  }));
  session.on('close', common.mustCall(function() {
    server.close();
  }));
}));

server.listen(0, common.mustCall(function() {
  const port = server.address().port;
  server.once('request', common.mustCall(function (request, resp) {
    response = resp;
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
      client.close();
    }));
    request.end();
    request.resume();
  }));
  client.on('goaway', common.mustCallAtLeast(1));
}));
