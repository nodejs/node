'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

{
  const server = http.createServer(common.mustNotCall());

  server.on('connect', common.mustCall((req, socket) => {
    assert.strictEqual(req.url, 'example.com');
    socket.end('HTTP/1.1 501 Not Implemented\r\n\r\n');
  }));

  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const req = http.request(
      new URL(`http://localhost:${port}/example.com`),
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
    assert.strictEqual(req.url, '/example.com');
    socket.end('HTTP/1.1 501 Not Implemented\r\n\r\n');
  }));

  server.listen(0, common.mustCall(() => {
    const req = http.request({
      host: 'localhost',
      port: server.address().port,
      method: 'CONNECT',
      path: '/example.com',
    });

    req.on('connect', common.mustCall((res, socket) => {
      assert.strictEqual(res.statusCode, 501);
      socket.destroy();
      server.close();
    }));

    req.end();
  }));
}
