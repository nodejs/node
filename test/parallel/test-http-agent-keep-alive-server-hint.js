'use strict';

// Regression test for https://github.com/nodejs/node/issues/66734
//
// When http.Agent is created with keepAlive: true but no explicit timeout,
// the server's Keep-Alive: timeout=N hint must be applied to the idle socket.
// Previously, agentTimeout defaulted to 0 and the condition
// `serverHintTimeout < agentTimeout` was never true, so setTimeout(0) was
// called, disabling the timer and leaving stale sockets in freeSockets.

const common = require('../common');
const assert = require('assert');
const http = require('http');

// Helper: returns the first idle socket for the agent, after the 'free'
// event has been processed (one setImmediate tick after response end).
function getIdleSocket(agent) {
  const key = Object.keys(agent.freeSockets)[0];
  return key ? agent.freeSockets[key][0] : undefined;
}

// Case 1: Agent has no explicit timeout.
// Server sends Keep-Alive: timeout=5.
// Expected: socket.timeout === 5000 - 1000 (default buffer) === 4000.
{
  const SERVER_HINT_S = 5;
  const EXPECTED_TIMEOUT = (SERVER_HINT_S * 1000) - 1000; // 4000ms

  const server = http.createServer(common.mustCall((req, res) => {
    res.writeHead(200, { 'Keep-Alive': `timeout=${SERVER_HINT_S}` });
    res.end('ok');
  }));

  server.listen(0, common.mustCall(() => {
    const agent = new http.Agent({ keepAlive: true });

    http.get({ port: server.address().port, agent }, common.mustCall((res) => {
      res.resume();
      res.on('end', common.mustCall(() => {
        setImmediate(common.mustCall(() => {
          const socket = getIdleSocket(agent);
          assert.ok(socket, 'socket should be in freeSockets');
          assert.strictEqual(
            socket.timeout,
            EXPECTED_TIMEOUT,
            `Case 1: expected socket.timeout ${EXPECTED_TIMEOUT}, ` +
            `got ${socket.timeout}`
          );
          agent.destroy();
          server.close();
        }));
      }));
    }));
  }));
}

// Case 2: Agent has an explicit timeout longer than the server hint.
// Server sends Keep-Alive: timeout=5 (4000ms after buffer).
// Agent timeout is 10000ms.
// Expected: server hint wins → socket.timeout === 4000.
{
  const SERVER_HINT_S = 5;
  const AGENT_TIMEOUT = 10000;
  const EXPECTED_TIMEOUT = (SERVER_HINT_S * 1000) - 1000; // 4000ms

  const server = http.createServer(common.mustCall((req, res) => {
    res.writeHead(200, { 'Keep-Alive': `timeout=${SERVER_HINT_S}` });
    res.end('ok');
  }));

  server.listen(0, common.mustCall(() => {
    const agent = new http.Agent({ keepAlive: true, timeout: AGENT_TIMEOUT });

    http.get({ port: server.address().port, agent }, common.mustCall((res) => {
      res.resume();
      res.on('end', common.mustCall(() => {
        setImmediate(common.mustCall(() => {
          const socket = getIdleSocket(agent);
          assert.ok(socket, 'socket should be in freeSockets');
          assert.strictEqual(
            socket.timeout,
            EXPECTED_TIMEOUT,
            `Case 2: expected socket.timeout ${EXPECTED_TIMEOUT}, ` +
            `got ${socket.timeout}`
          );
          agent.destroy();
          server.close();
        }));
      }));
    }));
  }));
}

// Case 3: Agent has an explicit timeout shorter than the server hint.
// Server sends Keep-Alive: timeout=10 (9000ms after buffer).
// Agent timeout is 3000ms.
// Expected: agent timeout wins → socket.timeout === 3000.
{
  const SERVER_HINT_S = 10;
  const AGENT_TIMEOUT = 3000;
  const EXPECTED_TIMEOUT = AGENT_TIMEOUT; // 3000ms

  const server = http.createServer(common.mustCall((req, res) => {
    res.writeHead(200, { 'Keep-Alive': `timeout=${SERVER_HINT_S}` });
    res.end('ok');
  }));

  server.listen(0, common.mustCall(() => {
    const agent = new http.Agent({ keepAlive: true, timeout: AGENT_TIMEOUT });

    http.get({ port: server.address().port, agent }, common.mustCall((res) => {
      res.resume();
      res.on('end', common.mustCall(() => {
        setImmediate(common.mustCall(() => {
          const socket = getIdleSocket(agent);
          assert.ok(socket, 'socket should be in freeSockets');
          assert.strictEqual(
            socket.timeout,
            EXPECTED_TIMEOUT,
            `Case 3: expected socket.timeout ${EXPECTED_TIMEOUT}, ` +
            `got ${socket.timeout}`
          );
          agent.destroy();
          server.close();
        }));
      }));
    }));
  }));
}

// Case 4: Server sends no Keep-Alive hint (keepAliveTimeout: 0 suppresses the
// automatic header), no agent timeout.
// Expected: socket.timeout === 0 (no timer set — existing behaviour unchanged).
{
  const server = http.createServer(
    { keepAliveTimeout: 0 },
    common.mustCall((req, res) => {
      res.end('ok');
    })
  );

  server.listen(0, common.mustCall(() => {
    const agent = new http.Agent({ keepAlive: true });

    http.get({ port: server.address().port, agent }, common.mustCall((res) => {
      res.resume();
      res.on('end', common.mustCall(() => {
        setImmediate(common.mustCall(() => {
          const socket = getIdleSocket(agent);
          assert.ok(socket, 'socket should be in freeSockets');
          assert.strictEqual(
            socket.timeout,
            0,
            `Case 4: expected socket.timeout 0, got ${socket.timeout}`
          );
          agent.destroy();
          server.close();
        }));
      }));
    }));
  }));
}
