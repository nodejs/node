'use strict';
require('../common');
const assert = require('assert');
const http = require('http');

const MAX_SOCKETS = 2;

const agent = new http.Agent({
  keepAlive: true,
  keepAliveMsecs: 1000,
  maxSockets: MAX_SOCKETS,
  maxFreeSockets: 2
});

const server = http.createServer(function(req, res) {
  res.end('hello world');
});

function get(path, callback) {
  return http.get({
    host: 'localhost',
    port: server.address().port,
    agent: agent,
    path: path
  }, callback);
}

server.listen(0, function() {
  var finished = 0;
  const num_requests = 6;
  for (var i = 0; i < num_requests; i++) {
    const request = get('/1', function() {
    });
    request.on('response', function() {
      request.abort();
      const sockets = agent.sockets[Object.keys(agent.sockets)[0]];
      assert(sockets.length <= MAX_SOCKETS);
      if (++finished === num_requests) {
        server.close();
      }
    });
  }
});
