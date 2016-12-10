'use strict';
var common = require('../common');
var http = require('http');
var assert = require('assert');

var N = 20;
var responses = 0;
var maxQueued = 0;

var agent = http.globalAgent;
agent.maxSockets = 10;

var server = http.createServer(function(req, res) {
  res.writeHead(200);
  res.end('Hello World\n');
});

var addrString = agent.getName({ host: '127.0.0.1', port: common.PORT });

server.listen(common.PORT, '127.0.0.1', function() {
  for (var i = 0; i < N; i++) {
    var options = {
      host: '127.0.0.1',
      port: common.PORT
    };

    var req = http.get(options, function(res) {
      if (++responses === N) {
        server.close();
      }
      res.resume();
    });

    assert.equal(req.agent, agent);

    console.log('Socket: ' + agent.sockets[addrString].length + '/' +
                agent.maxSockets + ' queued: ' + (agent.requests[addrString] ?
                agent.requests[addrString].length : 0));

    var agentRequests = agent.requests[addrString] ?
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
