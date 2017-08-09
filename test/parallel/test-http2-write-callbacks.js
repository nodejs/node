// Flags: --expose-http2
'use strict';

// Verifies that write callbacks are called

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const server = http2.createServer();

server.on('stream', common.mustCall((stream) => {
  stream.write('abc', common.mustCall(() => {
    stream.end('xyz');
  }));
  let actual = '';
  stream.setEncoding('utf8');
  stream.on('data', (chunk) => actual += chunk);
  stream.on('end', common.mustCall(() => assert.strictEqual(actual, 'abcxyz')));
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  const req = client.request({ ':method': 'POST' });
  req.write('abc', common.mustCall(() => {
    req.end('xyz');
  }));
  let actual = '';
  req.setEncoding('utf8');
  req.on('data', (chunk) => actual += chunk);
  req.on('end', common.mustCall(() => assert.strictEqual(actual, 'abcxyz')));
  req.on('streamClosed', common.mustCall(() => {
    client.destroy();
    server.close();
  }));
}));
