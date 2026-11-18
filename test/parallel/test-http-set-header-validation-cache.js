'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

{
  // Rotate the name cache through several generations.
  const msg = new http.OutgoingMessage();
  for (let i = 0; i < 1000; i++) {
    msg.setHeader(`X-Many-${i}`, 'v');
  }
  msg.setHeader('X-Many-0', 'evicted');
  msg.setHeader('X-Many-999', 'cached');
  assert.strictEqual(msg.getHeader('x-many-0'), 'evicted');
  assert.strictEqual(msg.getHeader('x-many-999'), 'cached');
  assert.throws(() => {
    msg.setHeader('Bad Header', 'v');
  }, { code: 'ERR_INVALID_HTTP_TOKEN' });
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
