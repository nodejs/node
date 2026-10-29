'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const { defaultMaxListeners } = require('events');

// Immediately destroying an HTTP request must not leak listeners on the
// freed socket. When sockets are reused via a keep-alive agent leaked
// listeners would accumulate to trigger a MaxListenersExceededWarning.

// Check we don't get a MaxListenersExceededWarning:
process.on('warning', common.mustNotCall('Unexpected warning emitted'));

const server = http.createServer(common.mustNotCall());

server.listen(0, common.mustCall(() => {
  const agent = new http.Agent({ keepAlive: true });
  const port = server.address().port;

  // Count actual socket creations to confirm reuse:
  let createSocketCount = 0;
  const origCreateSocket = agent.createSocket.bind(agent);
  agent.createSocket = function(...args) {
    createSocketCount++;
    return origCreateSocket(...args);
  };

  function executeHttpGet() {
    return new Promise((resolve) => {
      const req = http.get({ host: '127.0.0.1', port, agent });
      req.on('error', resolve);
      req.on('close', resolve);
      req.on('response', common.mustNotCall());
      req.destroy();
    });
  }

  async function main() {
    for (let i = 0; i < defaultMaxListeners + 1; i++) {
      await executeHttpGet();
    }

    assert.strictEqual(createSocketCount, 1);

    server.close();
    agent.destroy();
  }

  main().then(common.mustCall());
}));
