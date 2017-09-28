'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

let socketsCreated = 0;

class Agent extends http.Agent {
  createConnection(options, oncreate) {
    const socket = super.createConnection(options, oncreate);
    socketsCreated++;
    return socket;
  }
}

const server = http.createServer((req, res) => res.end());

const socketPath = common.PIPE;
common.refreshTmpDir();

server.listen(socketPath, common.mustCall(() => {
  const agent = new Agent({
    keepAlive: true,
    maxSockets: 1
  });

  http.get({ agent, socketPath }, (res) => res.resume());

  const req = http.get({ agent, socketPath }, common.mustNotCall());
  req.abort();

  http.get({ agent, socketPath }, common.mustCall((res) => {
    res.resume();
    assert.strictEqual(socketsCreated, 1);
    agent.destroy();
    server.close();
  }));
}));
