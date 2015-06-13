'use strict';
var common = require('../common');
var assert = require('assert');

var Buffer = require('buffer').Buffer,
    fs = require('fs'),
    dgram = require('dgram'), server, client,
    server_path = '/tmp/dgram_server_sock',
    messages_to_send = [
      new Buffer('First message to send'),
      new Buffer('Second message to send'),
      new Buffer('Third message to send'),
      new Buffer('Fourth message to send')
    ],
    timer;

server = dgram.createSocket('unix_dgram');
server.bind(server_path);
server.messages = [];
server.on('message', function(msg, rinfo) {
  console.log('server got: ' + msg);
  assert.strictEqual(rinfo.address, ''); // anon client sending
  server.messages.push(msg.toString());
  if (server.messages.length === messages_to_send.length) {
    server.messages.forEach(function(m, i) {
      assert.strictEqual(m, messages_to_send[i].toString());
    });
    server.close();
    client.close();
  }
});
server.on('listening', function() {
  console.log('server is listening');
  client = dgram.createSocket('unix_dgram');
  messages_to_send.forEach(function(m) {
    client.send(m, 0, m.length, server_path, function(err, bytes) {
      if (err) {
        console.log('Caught error in client send.');
        throw err;
      }
      console.log('client wrote ' + bytes + ' bytes.');
    });
  });
  client.on('close', function() {
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

timer = setTimeout(function() {
  throw new Error('Timeout');
}, 500);
