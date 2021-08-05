'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

const server = dgram.createSocket('udp4');
const message_to_send = 'A message to send';

server.on('message', common.mustCall((msg, rinfo) => {
  assert.strictEqual(rinfo.address, common.localhostIPv4);
  assert.strictEqual(msg.toString(), message_to_send.toString());
  server.send(msg, 0, msg.length, rinfo.port, rinfo.address);
}));

server.bind(0, 'localhost', common.mustCall(function() {
  const client = dgram.createSocket('udp4');
  const port = server.address().port;
  client.on('message', common.mustCall((msg, rinfo) => {
    assert.strictEqual(rinfo.address, common.localhostIPv4);
    assert.strictEqual(rinfo.port, port);
    assert.strictEqual(msg.toString(), message_to_send.toString());
    client.close();
    server.close();
  }));

  client.on('lookup', common.mustCall(function(err, ip, type, host) {
    assert.strictEqual(err, null);
    assert.strictEqual(ip, '127.0.0.1');
    assert.strictEqual(type, 4);
    assert.strictEqual(host, 'localhost');
  }));

  server.on('lookup', common.mustNotCall());

  client.send(message_to_send,
              0,
              message_to_send.length,
              port,
              'localhost');
  client.on('close', common.mustCall());
}));
