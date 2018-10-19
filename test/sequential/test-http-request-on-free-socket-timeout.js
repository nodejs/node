'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer(common.mustCallAtLeast((req, res) => {
  const content = 'Hello Agent';
  res.writeHead(200, {
    'Content-Length': content.length.toString(),
  });
  res.write(content);
  res.end();
}, 2));

server.listen(0, common.mustCall(() => {
  const agent = new http.Agent({ timeout: 100, keepAlive: true });
  const req = http.get({
    path: '/',
    port: server.address().port,
    agent
  }, common.mustCall((res) => {
    assert.strictEqual(res.statusCode, 200);
    res.resume();
    res.on('end', common.mustCall());
  }));

  const timer = setTimeout(() => {
    assert.fail('should not timeout here');
    req.abort();
  }, 1000);

  req.on('socket', common.mustCall((socket) => {
    // wait free socket become free and timeout
    socket.on('timeout', common.mustCall(() => {
      // free socket should be destroyed
      assert.strictEqual(socket.writable, false);
      // send new request will be fails
      clearTimeout(timer);
      const newReq = http.get({
        path: '/',
        port: server.address().port,
        agent
      }, common.mustCall((res) => {
        // agent must create a new socket to handle request
        assert.notStrictEqual(newReq.socket, socket);
        assert.strictEqual(res.statusCode, 200);
        res.resume();
        res.on('end', common.mustCall(() => server.close()));
      }));
    }));
  }));
}));
