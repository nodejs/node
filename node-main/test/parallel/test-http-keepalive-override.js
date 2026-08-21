'use strict';
const common = require('../common');
const assert = require('assert');

const http = require('http');

const server = http.createServer((req, res) => {
  res.end('ok');
}).listen(0, common.mustCall(() => {
  const agent = http.Agent({
    keepAlive: true,
    maxSockets: 5,
    maxFreeSockets: 2
  });

  const keepSocketAlive = agent.keepSocketAlive;
  const reuseSocket = agent.reuseSocket;

  let called = 0;
  let expectedSocket;
  agent.keepSocketAlive = common.mustCall((socket) => {
    assert(socket);

    called++;
    if (called === 1) {
      return false;
    } else if (called === 2) {
      expectedSocket = socket;
      return keepSocketAlive.call(agent, socket);
    }

    assert.strictEqual(socket, expectedSocket);
    return false;
  }, 3);

  agent.reuseSocket = common.mustCall((socket, req) => {
    assert.strictEqual(socket, expectedSocket);
    assert(req);

    return reuseSocket.call(agent, socket, req);
  }, 1);

  function req(callback) {
    http.request({
      method: 'GET',
      path: '/',
      agent,
      port: server.address().port
    }, common.mustCall((res) => {
      res.resume();
      res.once('end', common.mustCall(() => {
        setImmediate(callback);
      }));
    })).end();
  }

  // Should destroy socket instead of keeping it alive
  req(common.mustCall(() => {
    // Should keep socket alive
    req(common.mustCall(() => {
      // Should reuse the socket
      req(common.mustCall(() => {
        server.close();
      }));
    }));
  }));
}));
