// Flags: --expose-internals
'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

if (!common.hasMultiLocalhost())
  common.skip('platform-specific test.');

const http2 = require('http2');
const assert = require('assert');

const server = http2.createServer((req, res) => {
  console.log(`Connect from: ${req.connection.remoteAddress}`);
  assert.match(req.connection.remoteAddress, /^(127\.0\.0\.2|::1)$/);

  req.on('end', common.mustCall(() => {
    res.writeHead(200, { 'Content-Type': 'text/plain' });
    res.end(`You are from: ${req.connection.remoteAddress}`);
  }));
  req.resume();
});

server.listen(0, 'localhost', common.mustCall(() => {
  const options = { localAddress: common.hasIPv6 ? '::1' : '127.0.0.2' };

  const client = http2.connect(
    'http://localhost:' + server.address().port,
    options
  );
  const req = client.request({
    ':path': '/'
  });
  req.on('data', () => req.resume());
  req.on('end', common.mustCall(function() {
    client.close();
    req.close();
    server.close();
  }));
  req.end();
}));
