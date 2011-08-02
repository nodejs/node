var common = require('../common');
var http = require('http');
var assert = require('assert');

var N = 20;
var responses = 0;
var maxQueued = 0;

var agent = http.globalAgent;
agent.maxSockets = 10;

var server = http.createServer(function (req, res) {
  res.writeHead(200);
  res.end('Hello World\n');
});

server.listen(common.PORT, "127.0.0.1", function() {
  for (var i = 0; i < N; i++) {
    var options = {
      host: '127.0.0.1',
      port: common.PORT,
    };

    var req = http.get(options, function(res) {
      if (++responses == N) {
        server.close();
      }
    });

    assert.equal(req.agent, agent);

    console.log('Socket: ' + agent.sockets['127.0.0.1:'+common.PORT].length +
                '/' + agent.maxSockets +
                ' queued: '+ (agent.requests['127.0.0.1:'+common.PORT] ? agent.requests['127.0.0.1:'+common.PORT].length : 0));

    if (maxQueued < (agent.requests['127.0.0.1:'+common.PORT] ? agent.requests['127.0.0.1:'+common.PORT].length : 0))  {
      maxQueued = (agent.requests['127.0.0.1:'+common.PORT] ? agent.requests['127.0.0.1:'+common.PORT].length : 0);
    }
  }
});

process.on('exit', function() {
  assert.ok(responses == N);
  assert.ok(maxQueued <= 10);
});
