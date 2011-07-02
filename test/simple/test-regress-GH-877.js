var common = require('../common');
var http = require('http');
var assert = require('assert');

var N = 20;
var responses = 0;
var maxQueued = 0;

debugger;

var agent = http.getAgent('127.0.0.1', common.PORT);
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

    debugger;

    var req = http.get(options, function(res) {
      if (++responses == N) {
        server.close();
      }
    });

    assert.equal(req.agent, agent);

    console.log('Socket: ' + agent.sockets.length +
                '/' + agent.maxSockets +
                ' queued: '+ agent.queue.length);

    if (maxQueued < agent.queue.length)  {
      maxQueued = agent.queue.length;
    }
  }
});

process.on('exit', function() {
  assert.ok(responses == N);
  assert.ok(maxQueued <= 10);
});
