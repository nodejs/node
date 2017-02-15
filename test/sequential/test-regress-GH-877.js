'use strict';
const common = require('../common');
const http = require('http');
const assert = require('assert');

const N = 20;
let responses = 0;
let maxQueued = 0;

const agent = http.globalAgent;
agent.maxSockets = 10;

const server = http.createServer(function(req, res) {
  res.writeHead(200);
  res.end('Hello World\n');
});

const addrString = agent.getName({ host: '127.0.0.1', port: common.PORT });

server.listen(common.PORT, '127.0.0.1', function() {
  for (let i = 0; i < N; i++) {
    const options = {
      host: '127.0.0.1',
      port: common.PORT
    };

    const req = http.get(options, function(res) {
      if (++responses === N) {
        server.close();
      }
      res.resume();
    });

    assert.strictEqual(req.agent, agent);

    console.log('Socket: ' + agent.sockets[addrString].length + '/' +
                agent.maxSockets + ' queued: ' + (agent.requests[addrString] ?
                agent.requests[addrString].length : 0));

    const agentRequests = agent.requests[addrString] ?
        agent.requests[addrString].length : 0;

    if (maxQueued < agentRequests) {
      maxQueued = agentRequests;
    }
  }
});

process.on('exit', function() {
  assert.strictEqual(responses, N);
  assert.ok(maxQueued <= 10);
});
