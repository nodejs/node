'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

{
  // Rotate the name cache through several generations.
  const msg = new http.OutgoingMessage();
  for (let i = 0; i < 5000; i++) {
    msg.setHeader(`X-Many-${i}`, 'v');
  }
  msg.setHeader('X-Many-0', 'evicted');
  msg.setHeader('X-Many-4999', 'cached');
  assert.strictEqual(msg.getHeader('x-many-0'), 'evicted');
  assert.strictEqual(msg.getHeader('x-many-4999'), 'cached');
  // Names that were not admitted to the cache still validate values.
  for (let i = 0; i < 8; i++) {
    assert.throws(() => {
      msg.setHeader(`X-Uncached-${i}`, 'bad\u0007value');
    }, { code: 'ERR_INVALID_CHAR' });
  }
  assert.throws(() => {
    msg.setHeader('Bad Header', 'v');
  }, { code: 'ERR_INVALID_HTTP_TOKEN' });
}

{
  // Values of headers carrying credentials are not cached, and are validated
  // on every call.
  for (const name of ['Authorization', 'Cookie', 'Set-Cookie', 'set-cookie']) {
    const msg = new http.OutgoingMessage();
    // Set the name often enough for it to be admitted to the name cache.
    for (let i = 0; i < 16; i++) {
      msg.setHeader(name, 'secret=1');
    }
    assert.throws(() => {
      msg.setHeader(name, 'bad\u0007value');
    }, { code: 'ERR_INVALID_CHAR' });
    msg.setHeader(name, 'secret=1');
    assert.strictEqual(msg.getHeader(name), 'secret=1');
    msg.appendHeader(name, 'secret=2');
    assert.deepStrictEqual(msg.getHeader(name), ['secret=1', 'secret=2']);
  }
}

const server = http.createServer(common.mustCall((req, res) => {
  res.setHeader('Content-Type', 'application/json; charset=utf-8');
  res.setHeader('X-Test', 'valid-value');
  res.setHeader('Content-Type', 'application/json; charset=utf-8');
  res.setHeader('X-Test', 'valid-value');

  assert.throws(() => {
    res.setHeader('X-Test', 'bad\u0007value');
  }, { code: 'ERR_INVALID_CHAR' });

  const cookies = ['a=1', 'b=2'];
  res.setHeader('Set-Cookie', cookies);
  cookies[1] = 'bad\r\nvalue';
  assert.throws(() => {
    res.setHeader('Set-Cookie', cookies);
  }, { code: 'ERR_INVALID_CHAR' });
  res.setHeader('Set-Cookie', 'ok=1');

  assert.throws(() => {
    res.setHeader('Bad Header', 'v');
  }, { code: 'ERR_INVALID_HTTP_TOKEN' });

  res.end('ok');
}));

server.listen(0, common.mustCall(() => {
  http.get({ port: server.address().port }, common.mustCall((res) => {
    assert.strictEqual(res.statusCode, 200);
    res.resume();
    res.on('end', common.mustCall(() => server.close()));
  }));
}));
