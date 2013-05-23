// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

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
      if (++responses == N) {
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
  assert.ok(responses == N);
  assert.ok(maxQueued <= 10);
});
