'use strict';

const common = require('../common');
const assert = require('assert');

const http = require('http');

for (const method of ['abort', 'destroy']) {
  const server = http.createServer(common.mustCall((req, res) => {
    res.end(req.url);
  }));
  server.listen(0, common.mustCall(() => {
    const agent = http.Agent({ keepAlive: true });

    const req = http
      .request({
        port: server.address().port,
        agent
      })
      .on('socket', common.mustCall((socket) => {
        socket.on('free', common.mustCall());
      }))
      .on('response', common.mustCall((res) => {
        assert.strictEqual(req.destroyed, false);
        res.on('end', () => {
          assert.strictEqual(req.destroyed, true);
          req[method]();
          assert.strictEqual(req.socket.destroyed, false);
          agent.destroy();
          server.close();
        }).resume();
      }))
      .end();
    assert.strictEqual(req.destroyed, false);
  }));
}
