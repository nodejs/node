var common = require('../common');
var assert = require('assert');

var Buffer = require('buffer').Buffer,
    fs = require('fs'),
    dgram = require('dgram'), server, client,
    server_port = 20989,
    message_to_send = new Buffer('A message to send'),
    timer;

server = dgram.createSocket('udp4');
server.on('message', function(msg, rinfo) {
  console.log('server got: ' + msg +
              ' from ' + rinfo.address + ':' + rinfo.port);
  assert.strictEqual(rinfo.address, '127.0.0.1');
  assert.strictEqual(msg.toString(), message_to_send.toString());
  server.send(msg, 0, msg.length, rinfo.port, rinfo.address);
});
server.on('listening', function() {
  var address = server.address();
  console.log('server is listening on ' + address.address + ':' + address.port);
  client = dgram.createSocket('udp4');
  client.on('message', function(msg, rinfo) {
    console.log('client got: ' + msg +
                ' from ' + rinfo.address + ':' + address.port);
    assert.strictEqual(rinfo.address, '127.0.0.1');
    assert.strictEqual(rinfo.port, server_port);
    assert.strictEqual(msg.toString(), message_to_send.toString());
    client.close();
    server.close();
  });
  client.send(message_to_send, 0, message_to_send.length,
              server_port, 'localhost', function(err, bytes) {
        if (err) {
          console.log('Caught error in client send.');
          throw err;
        }
        console.log('client wrote ' + bytes + ' bytes.');
        assert.strictEqual(bytes, message_to_send.length);
      });
  client.on('close',
            function() {
              if (server.fd === null) {
                clearTimeout(timer);
              }
            });
});
server.on('close', function() {
  if (client.fd === null) {
    clearTimeout(timer);
  }
});
server.bind(server_port);

timer = setTimeout(function() {
  throw new Error('Timeout');
}, 200);
