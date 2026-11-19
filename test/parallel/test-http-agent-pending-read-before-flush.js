// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const Agent = require('internal/http/agent');

const server = http.createServer(common.mustCall((req, res) => {
  res.end();
}, 1));

server.listen(0, common.mustCall(() => {
  const agent = new http.Agent({ keepAlive: true });
  const options = {
    host: '127.0.0.1',
    port: server.address().port,
    agent,
  };
  const name = agent.getName(options);
  const request1 = http.get(options, common.mustCall((response) => {
    response.resume();
  }));

  request1.on('close', common.mustCall(() => {
    process.nextTick(common.mustCall(() => {
      assert.strictEqual(agent.freeSockets[name]?.length, 1);

      // The Agent has already selected a clean socket. Simulate data arriving
      // before the client installs its parser and flushes the next request.
      Agent.hasPendingRead = common.mustCall(() => true);

      const request2 = http.get(options, common.mustNotCall());
      request2.on('error', common.mustCall((err) => {
        assert.strictEqual(err.code, 'ECONNRESET');
        agent.destroy();
        server.close();
      }));
    }));
  }));
}));
