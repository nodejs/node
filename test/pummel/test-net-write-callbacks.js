'use strict';
var common = require('../common');
var net = require('net');
var assert = require('assert');

var cbcount = 0;
var N = 500000;

var server = net.Server(function(socket) {
  socket.on('data', function(d) {
    console.error('got %d bytes', d.length);
  });

  socket.on('end', function() {
    console.error('end');
    socket.destroy();
    server.close();
  });
});

var lastCalled = -1;
function makeCallback(c) {
  var called = false;
  return function() {
    if (called)
      throw new Error('called callback #' + c + ' more than once');
    called = true;
    if (c < lastCalled)
      throw new Error('callbacks out of order. last=' + lastCalled +
                      ' current=' + c);
    lastCalled = c;
    cbcount++;
  };
}

server.listen(common.PORT, function() {
  var client = net.createConnection(common.PORT);

  client.on('connect', function() {
    for (var i = 0; i < N; i++) {
      client.write('hello world', makeCallback(i));
    }
    client.end();
  });
});

process.on('exit', function() {
  assert.equal(N, cbcount);
});
