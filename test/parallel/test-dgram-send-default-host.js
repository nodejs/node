'use strict';

const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

const client = dgram.createSocket('udp4');

const toSend = [Buffer.alloc(256, 'x'),
                Buffer.alloc(256, 'y'),
                Buffer.alloc(256, 'z'),
                'hello'];

const received = [];

client.on('listening', common.mustCall(() => {
  client.send(toSend[0], 0, toSend[0].length, common.PORT);
  client.send(toSend[1], common.PORT);
  client.send([toSend[2]], common.PORT);
  client.send(toSend[3], 0, toSend[3].length, common.PORT);
}));

client.on('message', common.mustCall((buf, info) => {
  received.push(buf.toString());

  if (received.length === toSend.length) {
    // The replies may arrive out of order -> sort them before checking.
    received.sort();

    const expected = toSend.map(String).sort();
    assert.deepStrictEqual(received, expected);
    client.close();
  }
}, toSend.length));

client.bind(common.PORT);
