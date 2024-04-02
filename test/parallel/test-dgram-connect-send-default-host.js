'use strict';

const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

const client = dgram.createSocket('udp4');
const server = dgram.createSocket('udp4');

const toSend = [Buffer.alloc(256, 'x'),
                Buffer.alloc(256, 'y'),
                Buffer.alloc(256, 'z'),
                'hello'];

const received = [];

server.on('listening', common.mustCall(() => {
  const port = server.address().port;
  client.connect(port, (err) => {
    assert.ifError(err);
    client.send(toSend[0], 0, toSend[0].length);
    client.send(toSend[1]);
    client.send([toSend[2]]);
    client.send(toSend[3], 0, toSend[3].length);

    client.send(new Uint8Array(toSend[0]), 0, toSend[0].length);
    client.send(new Uint8Array(toSend[1]));
    client.send([new Uint8Array(toSend[2])]);
    client.send(new Uint8Array(Buffer.from(toSend[3])),
                0, toSend[3].length);
  });
}));

server.on('message', common.mustCall((buf, info) => {
  received.push(buf.toString());

  if (received.length === toSend.length * 2) {
    // The replies may arrive out of order -> sort them before checking.
    received.sort();

    const expected = toSend.concat(toSend).map(String).sort();
    assert.deepStrictEqual(received, expected);
    client.close();
    server.close();
  }
}, toSend.length * 2));

server.bind(0);
