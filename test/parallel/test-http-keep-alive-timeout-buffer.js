'use strict';

const common = require('../common');
const http = require('http');
const assert = require('assert');

const server = http.createServer(common.mustCall((req, res) => {
  const body = 'buffer test\n';

  res.writeHead(200, { 'Content-Length': body.length });
  res.write(body);
  res.end();
}));

server.keepAliveTimeout = 100;

if (server.keepAliveTimeoutBuffer === undefined) {
  server.keepAliveTimeoutBuffer = 1000;
}
assert.strictEqual(server.keepAliveTimeoutBuffer, 1000);

server.listen(0, () => {
  http.get({
    port: server.address().port,
    path: '/',
  }, (res) => {
    res.resume(); 
    server.close();
  });
});
