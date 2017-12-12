'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

const server = h2.createServer();

// we use the lower-level API here
server.on('stream', common.mustCall((stream, headers, flags) => {
  let data = '';
  stream.setEncoding('utf8');
  stream.on('data', (chunk) => data += chunk);
  stream.on('end', common.mustCall(() => {
    assert.strictEqual(data, 'some data more data');
  }));
  stream.respond();
  stream.end('ok');
}));

server.listen(0, common.mustCall(() => {
  const client = h2.connect(`http://localhost:${server.address().port}`);

  const req = client.request({ ':method': 'POST' });
  req.write('some data ');
  req.end('more data');

  req.on('response', common.mustCall());
  req.resume();
  req.on('end', common.mustCall());
  req.on('close', common.mustCall(() => {
    server.close();
    client.close();
  }));
}));
