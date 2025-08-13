'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

// Ensure agentKeepAliveTimeoutBuffer option sets the correct value or falls back to default.
{
  const agent1 = new http.Agent({ agentKeepAliveTimeoutBuffer: 1500, keepAlive: true });
  assert.strictEqual(agent1.agentKeepAliveTimeoutBuffer, 1500);

  const agent2 = new http.Agent({ agentKeepAliveTimeoutBuffer: -100, keepAlive: true });
  assert.strictEqual(agent2.agentKeepAliveTimeoutBuffer, 1000);

  const agent3 = new http.Agent({ agentKeepAliveTimeoutBuffer: Infinity, keepAlive: true });
  assert.strictEqual(agent3.agentKeepAliveTimeoutBuffer, 1000);

  const agent4 = new http.Agent({ keepAlive: true });
  assert.strictEqual(agent4.agentKeepAliveTimeoutBuffer, 1000);
}

// Integration test with server sending Keep-Alive timeout header.
{
  const SERVER_TIMEOUT = 3;
  const BUFFER = 1500;

  const server = http.createServer((req, res) => {
    res.setHeader('Keep-Alive', `timeout=${SERVER_TIMEOUT}`);
    res.end('ok');
  });

  server.listen(0, common.mustCall(() => {
    const agent = new http.Agent({ agentKeepAliveTimeoutBuffer: BUFFER, keepAlive: true });
    assert.strictEqual(agent.agentKeepAliveTimeoutBuffer, BUFFER);

    http.get({ port: server.address().port, agent }, (res) => {
      res.resume();
      res.on('end', () => {
        agent.destroy();
        server.close();
      });
    });
  }));
}
