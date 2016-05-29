'use strict';
require('../common');
var assert = require('assert');
var http = require('http');

var name;
var max = 3;
var count = 0;

var server = http.Server(function(req, res) {
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
  for (var i = 0; i < max; ++i) {
    request(i);
  }
});

function request(i) {
  var req = http.get({
    port: server.address().port,
    path: '/' + i
  }, function(res) {
    var socket = req.socket;
    socket.on('close', function() {
      ++count;
      if (count < max) {
        assert.equal(http.globalAgent.sockets[name].indexOf(socket), -1);
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
  assert.equal(count, max);
});
