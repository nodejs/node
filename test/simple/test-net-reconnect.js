var common = require('../common');
var assert = require('assert');

var net = require('net');

var N = 50;
var c = 0;
var client_recv_count = 0;
var disconnect_count = 0;

var server = net.createServer(function(socket) {
  socket.addListener('connect', function() {
    socket.write('hello\r\n');
  });

  socket.addListener('end', function() {
    socket.end();
  });

  socket.addListener('close', function(had_error) {
    //console.log('server had_error: ' + JSON.stringify(had_error));
    assert.equal(false, had_error);
  });
});

server.listen(common.PORT, function() {
  console.log('listening');
  var client = net.createConnection(common.PORT);

  client.setEncoding('UTF8');

  client.addListener('connect', function() {
    console.log('client connected.');
  });

  client.addListener('data', function(chunk) {
    client_recv_count += 1;
    console.log('client_recv_count ' + client_recv_count);
    assert.equal('hello\r\n', chunk);
    client.end();
  });

  client.addListener('close', function(had_error) {
    console.log('disconnect');
    assert.equal(false, had_error);
    if (disconnect_count++ < N)
      client.connect(common.PORT); // reconnect
    else
      server.close();
  });
});

process.addListener('exit', function() {
  assert.equal(N + 1, disconnect_count);
  assert.equal(N + 1, client_recv_count);
});

