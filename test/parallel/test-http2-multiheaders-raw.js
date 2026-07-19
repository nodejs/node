'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const server = http2.createServer();

const src = { __proto__: null };
src['www-authenticate'] = 'foo';
src['WWW-Authenticate'] = 'bar';
src['WWW-AUTHENTICATE'] = 'baz';
src.test = 'foo, bar, baz';

server.on('stream', common.mustCall((stream, headers, flags, rawHeaders) => {
  const expected = [
    ':method',
    'GET',
    ':authority',
    `localhost:${server.address().port}`,
    ':scheme',
    'http',
    ':path',
    '/',
    'www-authenticate',
    'foo',
    'www-authenticate',
    'bar',
    'www-authenticate',
    'baz',
    'test',
    'foo, bar, baz',
  ];

  assert.deepStrictEqual(rawHeaders, expected);
  stream.respond(src);
  stream.end();
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  const req = client.request(src);
  req.on('close', common.mustCall(() => {
    server.close();
    client.close();
  }));
}));
