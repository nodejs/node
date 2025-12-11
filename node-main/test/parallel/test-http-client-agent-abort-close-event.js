'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer(common.mustNotCall());

const keepAliveAgent = new http.Agent({ keepAlive: true });

server.listen(0, common.mustCall(() => {
  const req = http.get({
    port: server.address().port,
    agent: keepAliveAgent
  });

  req
    .on('socket', common.mustNotCall())
    .on('response', common.mustNotCall())
    .on('close', common.mustCall(() => {
      assert.strictEqual(req.destroyed, true);
      server.close();
      keepAliveAgent.destroy();
    }))
    .abort();
}));
