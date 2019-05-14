'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

const server = h2.createServer();

const setCookie = [
  'a=b',
  'c=d; Wed, 21 Oct 2015 07:28:00 GMT; Secure; HttpOnly',
  'e=f'
];

// We use the lower-level API here
server.on('stream', common.mustCall(onStream));

function onStream(stream, headers, flags) {

  assert.strictEqual(typeof headers.abc, 'string');
  assert.strictEqual(headers.abc, '1, 2, 3');
  assert.strictEqual(typeof headers.cookie, 'string');
  assert.strictEqual(headers.cookie, 'a=b; c=d; e=f');

  stream.respond({
    'content-type': 'text/html',
    ':status': 200,
    'set-cookie': setCookie
  });

  stream.end('hello world');
}

server.listen(0);

server.on('listening', common.mustCall(() => {

  const client = h2.connect(`http://localhost:${server.address().port}`);

  const req = client.request({
    ':path': '/',
    'abc': [1, 2, 3],
    'cookie': ['a=b', 'c=d', 'e=f'],
  });
  req.resume();

  req.on('response', common.mustCall((headers) => {
    assert(Array.isArray(headers['set-cookie']));
    assert.deepStrictEqual(headers['set-cookie'], setCookie);
  }));

  req.on('end', common.mustCall(() => {
    server.close();
    client.close();
  }));
  req.end();

}));
