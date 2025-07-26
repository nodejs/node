'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');
const message_to_send = 'A message to send';

const server = dgram.createSocket({ type: 'udp4', msgCount: 3 });
const client = dgram.createSocket({ type: 'udp4' });

client.on('close', common.mustCall());
server.on('close', common.mustCall());

server.on('message', common.mustCall((msg) => {
  assert.strictEqual(msg.toString(), message_to_send.toString());
  // The server will release the memory which allocated by handle->alloc_cb
  server.close();
  client.close();
}));

server.on('listening', common.mustCall(() => {
  for (let i = 0; i < 2; i++) {
    client.send(message_to_send,
                0,
                message_to_send.length,
                server.address().port,
                server.address().address);
  }
}));

server.bind(0);
