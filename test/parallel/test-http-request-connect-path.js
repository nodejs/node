'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

for (const path of [
  '',
  'example.com',
  'example.com:0',
  'example.com:65536',
  'example.com:8080/example',
  'evil.com:666/good.org:777',
  '/example.com',
]) {
  assert.throws(() => http.request({
    method: 'CONNECT',
    path,
  }), {
    code: 'ERR_INVALID_ARG_VALUE',
    name: 'TypeError',
    message: /^The property 'options\.path' must be a valid host:port combo\./,
  });
}

{
  const server = http.createServer(common.mustNotCall());

  server.on('connect', common.mustCall((req, socket) => {
    assert.strictEqual(req.url, 'example.com:80');
    socket.end('HTTP/1.1 501 Not Implemented\r\n\r\n');
  }));

  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const req = http.request(
      new URL(`http://localhost:${port}/example.com:80`),
      { method: 'CONNECT' },
    );

    req.on('connect', common.mustCall((res, socket) => {
      assert.strictEqual(res.statusCode, 501);
      socket.destroy();
      server.close();
    }));

    req.end();
  }));
}

{
  const server = http.createServer(common.mustNotCall());

  server.on('connect', common.mustCall((req, socket) => {
    assert.strictEqual(req.url, '[2001:db8::1]:111');
    socket.end('HTTP/1.1 501 Not Implemented\r\n\r\n');
  }));

  server.listen(0, common.mustCall(() => {
    const req = http.request({
      host: 'localhost',
      port: server.address().port,
      method: 'CONNECT',
      path: '[2001:db8::1]:111',
    });

    req.on('connect', common.mustCall((res, socket) => {
      assert.strictEqual(res.statusCode, 501);
      socket.destroy();
      server.close();
    }));

    req.end();
  }));
}
