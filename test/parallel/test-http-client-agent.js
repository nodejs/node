'use strict';
require('../common');
const assert = require('assert');
const http = require('http');

let name;
const max = 3;
let count = 0;

const server = http.Server(function(req, res) {
  if (req.url === '/0') {
    setTimeout(function() {
      res.writeHead(200);
      res.end('Hello, World!');
    }, 100);
  } else {
    res.writeHead(200);
    res.end('Hello, World!');
  }
});
server.listen(0, function() {
  name = http.globalAgent.getName({ port: this.address().port });
  for (let i = 0; i < max; ++i) {
    request(i);
  }
});

function request(i) {
  const req = http.get({
    port: server.address().port,
    path: '/' + i
  }, function(res) {
    const socket = req.socket;
    socket.on('close', function() {
      ++count;
      if (count < max) {
        assert.strictEqual(http.globalAgent.sockets[name].indexOf(socket), -1);
      } else {
        assert(!http.globalAgent.sockets.hasOwnProperty(name));
        assert(!http.globalAgent.requests.hasOwnProperty(name));
        server.close();
      }
    });
    res.resume();
  });
}

process.on('exit', function() {
  assert.strictEqual(count, max);
});
