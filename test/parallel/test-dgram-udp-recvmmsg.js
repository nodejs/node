'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');
const message_to_send = 'A message to send';

[
  -1,
  1.1,
  NaN,
  undefined,
  {},
  [],
  null,
  function() {},
  Symbol(),
  true,
  Infinity,
].forEach((msgCount) => {
  try {
    dgram.createSocket({ type: 'udp4', msgCount });
  } catch (e) {
    assert.ok(/ERR_OUT_OF_RANGE|ERR_INVALID_ARG_TYPE/i.test(e.code));
  }
});

[
  0,
  1,
].forEach((msgCount) => {
  const socket = dgram.createSocket({ type: 'udp4', msgCount });
  socket.close();
});

const server = dgram.createSocket({ type: 'udp4', msgCount: 3 });
const client = dgram.createSocket({ type: 'udp4' });

client.on('close', common.mustCall());
server.on('close', common.mustCall());

let done = 0;
const count = 2;
server.on('message', common.mustCall((msg) => {
  assert.strictEqual(msg.toString(), message_to_send.toString());
  if (++done === count) {
    client.close();
    server.close();
  }
}, count));

server.on('listening', common.mustCall(() => {
  for (let i = 0; i < count; i++) {
    client.send(message_to_send,
                0,
                message_to_send.length,
                server.address().port,
                server.address().address);
  }
}));

server.bind(0);
