'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

// Check that pushStream handles being passed wrong arguments
// in the expected manner

const server = http2.createServer();
server.on('stream', common.mustCall((stream, headers) => {
  const port = server.address().port;

  // Must receive a callback (function)
  assert.throws(
    () => stream.pushStream({
      ':scheme': 'http',
      ':path': '/foobar',
      ':authority': `localhost:${port}`,
    }, {}, 'callback'),
    {
      code: 'ERR_INVALID_CALLBACK',
      message: "Callback must be a function. Received 'callback'"
    }
  );

  // Must validate headers
  assert.throws(
    () => stream.pushStream({ 'connection': 'test' }, {}, () => {}),
    {
      code: 'ERR_HTTP2_INVALID_CONNECTION_HEADERS',
      name: 'TypeError',
      message: 'HTTP/1 Connection specific headers are forbidden: "connection"'
    }
  );

  stream.end('test');
}));

server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  const headers = { ':path': '/' };
  const client = http2.connect(`http://localhost:${port}`);
  const req = client.request(headers);
  req.setEncoding('utf8');

  let data = '';
  req.on('data', common.mustCall((d) => data += d));
  req.on('end', common.mustCall(() => {
    assert.strictEqual(data, 'test');
    server.close();
    client.close();
  }));
  req.end();
}));
