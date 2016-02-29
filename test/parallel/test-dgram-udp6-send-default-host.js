'use strict';

const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

if (!common.hasIPv6) {
  console.log('1..0 # Skipped: no IPv6 support');
  return;
}

const client = dgram.createSocket('udp6');

const toSend = [new Buffer(256), new Buffer(256), new Buffer(256), 'hello'];

toSend[0].fill('x');
toSend[1].fill('y');
toSend[2].fill('z');

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
