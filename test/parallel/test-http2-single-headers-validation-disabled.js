'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const server = http2.createServer({
  strictSingleValueFields: false
});

server.on('stream', common.mustCall((stream, _headers, _flags, rawHeaders) => {
  assert.deepStrictEqual(rawHeaders.slice(8), [
    'user-agent', 'abc',
    'user-agent', 'xyz',
    'referer', 'qwe',
    'referer', 'asd',
  ]);

  stream.respond({
    ':status': 200,
    'expires': 'Thu, 01 Jan 1970 00:00:00 GMT',
    'EXPIRES': 'Thu, 01 Jan 1970 00:00:00 GMT',
    'content-type': ['a', 'b'],
  });

  stream.end();
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`, {
    strictSingleValueFields: false
  });

  const res = client.request({
    'user-agent': 'abc',
    'USER-AGENT': 'xyz',
    'referer': ['qwe', 'asd'],
  });

  res.on('response', common.mustCall((_headers, _flags, rawHeaders) => {
    assert.deepStrictEqual(rawHeaders.slice(2, 10), [
      'expires', 'Thu, 01 Jan 1970 00:00:00 GMT',
      'expires', 'Thu, 01 Jan 1970 00:00:00 GMT',
      'content-type', 'a',
      'content-type', 'b',
    ]);

    server.close();
    client.close();
  }));
}));
