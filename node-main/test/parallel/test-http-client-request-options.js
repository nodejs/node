'use strict';

const common = require('../common');
const assert = require('node:assert');
const http = require('node:http');

const headers = { foo: 'Bar' };
const server = http.createServer(common.mustCall((req, res) => {
  assert.strictEqual(req.url, '/ping?q=term');
  assert.strictEqual(req.headers?.foo, headers.foo);
  req.resume();
  req.on('end', () => {
    res.writeHead(200);
    res.end('pong');
  });
}));

server.listen(0, common.localhostIPv4, () => {
  const { address, port } = server.address();
  const url = new URL(`http://${address}:${port}/ping?q=term`);
  url.headers = headers;
  const clientReq = http.request(url);
  clientReq.on('close', common.mustCall(() => {
    server.close();
  }));
  clientReq.end();
});
