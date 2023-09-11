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

server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  const agent = new Agent({
    keepAlive: true,
    maxSockets: 1
  });

  const req = http.get({ agent, port }, common.mustCall((res) => {
    res.resume();
    res.on('end', () => {
      res.destroy();

      http.get({ agent, port }, common.mustCall((res) => {
        res.resume();
        assert.strictEqual(socketsCreated, 1);
        agent.destroy();
        server.close();
      }));
    });
  }));
  req.end();
}));
