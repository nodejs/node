'use strict';
var common = require('../common');
var assert = require('assert');

var fs = require('fs'),
    dgram = require('dgram'), server, client,
    server_port = common.PORT,
    message_to_send = 'A message to send',
    timer;

server = dgram.createSocket('udp4');
server.on('message', function(msg, rinfo) {
  console.log('server got: ' + msg +
              ' from ' + rinfo.address + ':' + rinfo.port);
  assert.strictEqual(rinfo.address, common.localhostIPv4);
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
    assert.strictEqual(rinfo.address, common.localhostIPv4);
    assert.strictEqual(rinfo.port, server_port);
    assert.strictEqual(msg.toString(), message_to_send.toString());
    client.close();
    server.close();
  });
  client.send(message_to_send, 0, message_to_send.length,
              server_port, 'localhost', function(err) {
        if (err) {
          console.log('Caught error in client send.');
          throw err;
        }
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
