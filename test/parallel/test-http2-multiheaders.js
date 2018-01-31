'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const server = http2.createServer();

const src = Object.create(null);
src.accept = [ 'abc', 'def' ];
src.Accept = 'ghijklmnop';
src['www-authenticate'] = 'foo';
src['WWW-Authenticate'] = 'bar';
src['WWW-AUTHENTICATE'] = 'baz';
src['proxy-authenticate'] = 'foo';
src['Proxy-Authenticate'] = 'bar';
src['PROXY-AUTHENTICATE'] = 'baz';
src['x-foo'] = 'foo';
src['X-Foo'] = 'bar';
src['X-FOO'] = 'baz';
src.constructor = 'foo';
src.Constructor = 'bar';
src.CONSTRUCTOR = 'baz';
// eslint-disable-next-line no-proto
src['__proto__'] = 'foo';
src['__PROTO__'] = 'bar';
src['__Proto__'] = 'baz';

function checkHeaders(headers) {
  assert.deepStrictEqual(headers['accept'],
                         'abc, def, ghijklmnop');
  assert.deepStrictEqual(headers['www-authenticate'],
                         'foo, bar, baz');
  assert.deepStrictEqual(headers['proxy-authenticate'],
                         'foo, bar, baz');
  assert.deepStrictEqual(headers['x-foo'], 'foo, bar, baz');
  assert.deepStrictEqual(headers['constructor'], 'foo, bar, baz');
  // eslint-disable-next-line no-proto
  assert.deepStrictEqual(headers['__proto__'], 'foo, bar, baz');
}

server.on('stream', common.mustCall((stream, headers) => {
  assert.strictEqual(headers[':path'], '/');
  assert.strictEqual(headers[':scheme'], 'http');
  assert.strictEqual(headers[':method'], 'GET');
  checkHeaders(headers);
  stream.respond(src);
  stream.end();
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  const req = client.request(src);
  req.on('response', common.mustCall(checkHeaders));
  req.on('close', common.mustCall(() => {
    server.close();
    client.close();
  }));
}));
