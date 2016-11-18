'use strict';
require('../common');
var assert = require('assert');

var net = require('net');

var N = 50;
var client_recv_count = 0;
var client_end_count = 0;
var disconnect_count = 0;

var server = net.createServer(function(socket) {
  console.error('SERVER: got socket connection');
  socket.resume();

  console.error('SERVER connect, writing');
  socket.write('hello\r\n');

  socket.on('end', function() {
    console.error('SERVER socket end, calling end()');
    socket.end();
  });

  socket.on('close', function(had_error) {
    console.log('SERVER had_error: ' + JSON.stringify(had_error));
    assert.equal(false, had_error);
  });
});

server.listen(0, function() {
  console.log('SERVER listening');
  var client = net.createConnection(this.address().port);

  client.setEncoding('UTF8');

  client.on('connect', function() {
    console.error('CLIENT connected', client._writableState);
  });

  client.on('data', function(chunk) {
    client_recv_count += 1;
    console.log('client_recv_count ' + client_recv_count);
    assert.equal('hello\r\n', chunk);
    console.error('CLIENT: calling end', client._writableState);
    client.end();
  });

  client.on('end', function() {
    console.error('CLIENT end');
    client_end_count++;
  });

  client.on('close', function(had_error) {
    console.log('CLIENT disconnect');
    assert.equal(false, had_error);
    if (disconnect_count++ < N)
      client.connect(server.address().port); // reconnect
    else
      server.close();
  });
});

process.on('exit', function() {
  assert.equal(N + 1, disconnect_count);
  assert.equal(N + 1, client_recv_count);
  assert.equal(N + 1, client_end_count);
});
