'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

// Check that drain event is passed from Http2Stream

const testString = 'tests';

const server = h2.createServer();

server.on('request', common.mustCall((req, res) => {
  res.stream._writableState.highWaterMark = testString.length;
  assert.strictEqual(res.write(testString), false);
  res.on('drain', common.mustCall(() => res.end(testString)));
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
  request.end();

  let data = '';
  request.setEncoding('utf8');
  request.on('data', (chunk) => (data += chunk));

  request.on('end', common.mustCall(function() {
    assert.strictEqual(data, testString.repeat(2));
    client.close();
    server.close();
  }));
}));
