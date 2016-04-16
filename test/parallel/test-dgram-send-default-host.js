'use strict';

const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

const client = dgram.createSocket('udp4');

const toSend = [Buffer.alloc(256, 'x'),
                Buffer.alloc(256, 'y'),
                Buffer.alloc(256, 'z'),
                'hello'];

client.on('listening', function() {
  client.send(toSend[0], 0, toSend[0].length, common.PORT);
  client.send(toSend[1], common.PORT);
  client.send([toSend[2]], common.PORT);
  client.send(toSend[3], 0, toSend[3].length, common.PORT);
});

client.on('message', function(buf, info) {
  const expected = toSend.shift().toString();
  assert.ok(buf.toString() === expected, 'message was received correctly');

  if (toSend.length === 0) {
    client.close();
  }
});

client.bind(common.PORT);
