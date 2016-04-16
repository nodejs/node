'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');
const server_port = common.PORT;
const message_to_send = 'A message to send';

const server = dgram.createSocket('udp4');
server.on('message', common.mustCall((msg, rinfo) => {
  assert.strictEqual(rinfo.address, common.localhostIPv4);
  assert.strictEqual(msg.toString(), message_to_send.toString());
  server.send(msg, 0, msg.length, rinfo.port, rinfo.address);
}));
server.on('listening', common.mustCall(() => {
  const client = dgram.createSocket('udp4');
  client.on('message', common.mustCall((msg, rinfo) => {
    assert.strictEqual(rinfo.address, common.localhostIPv4);
    assert.strictEqual(rinfo.port, server_port);
    assert.strictEqual(msg.toString(), message_to_send.toString());
    client.close();
    server.close();
  }));
  client.send(message_to_send,
              0,
              message_to_send.length,
              server_port,
              'localhost');
  client.on('close', common.mustCall(() => {}));
}));
server.on('close', common.mustCall(() => {}));
server.bind(server_port);

