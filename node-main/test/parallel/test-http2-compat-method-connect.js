'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const server = http2.createServer(common.mustNotCall());

server.listen(0, common.mustCall(() => testMethodConnect(2)));

server.once('connect', common.mustCall((req, res) => {
  assert.strictEqual(req.headers[':method'], 'CONNECT');
  res.statusCode = 405;
  res.end();
}));

function testMethodConnect(testsToRun) {
  if (!testsToRun) {
    return server.close();
  }

  const port = server.address().port;
  const client = http2.connect(`http://localhost:${port}`);
  const req = client.request({
    ':method': 'CONNECT',
    ':authority': `localhost:${port}`
  });

  req.on('response', common.mustCall((headers) => {
    assert.strictEqual(headers[':status'], 405);
  }));
  req.resume();
  req.on('end', common.mustCall(() => {
    client.close();
    testMethodConnect(testsToRun - 1);
  }));
  req.end();
}
