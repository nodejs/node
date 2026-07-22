'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

// Check that pause & resume work as expected with Http2ServerRequest

const testStr = 'Request Body from Client';

const server = h2.createServer();

server.on('request', common.mustCall((req, res) => {
  let data = '';
  req.pause();
  req.setEncoding('utf8');
  req.on('data', common.mustCall((chunk) => (data += chunk)));
  setTimeout(common.mustCall(() => {
    assert.strictEqual(data, '');
    req.resume();
  }), common.platformTimeout(100));
  req.on('end', common.mustCall(() => {
    assert.strictEqual(data, testStr);
    res.end();
  }));

  // Shouldn't throw if underlying Http2Stream no longer exists
  res.on('finish', common.mustCall(() => process.nextTick(() => {
    req.pause();
    req.resume();
  })));
}));

server.listen(0, common.mustCall(() => {
  const port = server.address().port;

  const client = h2.connect(`http://localhost:${port}`);
  const request = client.request({
    ':path': '/foobar',
    ':method': 'POST',
    ':scheme': 'http',
    ':authority': `localhost:${port}`
  });
  request.resume();
  request.end(testStr);
  request.on('end', common.mustCall(function() {
    client.close();
    server.close();
  }));
}));
