'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const server = http2.createServer(function(request, response) {
  response.writeHead(200, { 'Content-Type': 'text/plain' });
  response.write('1\n');
  response.write('');
  response.write('2\n');
  response.write('');
  response.end('3\n');

  this.close();
});

server.listen(0, common.mustCall(function() {
  const client = http2.connect(`http://localhost:${this.address().port}`);
  const headers = { ':path': '/' };
  const req = client.request(headers).setEncoding('ascii');

  let res = '';

  req.on('response', common.mustCall(function(headers) {
    assert.strictEqual(headers[':status'], 200);
  }));

  req.on('data', (chunk) => {
    res += chunk;
  });

  req.on('end', common.mustCall(function() {
    assert.strictEqual(res, '1\n2\n3\n');
    client.close();
  }));

  req.end();
}));
