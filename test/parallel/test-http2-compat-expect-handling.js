'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const expectValue = 'meoww';

const server = http2.createServer(common.mustNotCall());

server.once('checkExpectation', common.mustCall((req, res) => {
  assert.strictEqual(req.headers['expect'], expectValue);
  res.statusCode = 417;
  res.end();
}));

server.listen(0, common.mustCall(() => nextTest(2)));

function nextTest(testsToRun) {
  if (!testsToRun) {
    return server.close();
  }

  const port = server.address().port;
  const client = http2.connect(`http://localhost:${port}`);
  const req = client.request({
    ':path': '/',
    ':method': 'GET',
    ':scheme': 'http',
    ':authority': `localhost:${port}`,
    expect: expectValue
  });

  req.on('response', common.mustCall((headers) => {
    assert.strictEqual(headers[':status'], 417);
    req.resume();
  }));

  req.on('end', common.mustCall(() => {
    client.close();
    nextTest(testsToRun - 1);
  }));
}
