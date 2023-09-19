'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');
const socket = dgram.createSocket('udp4');
const data = ['foo', 'bar', 'baz'];

socket.on('message', common.mustCall((msg, rinfo) => {
  socket.close();
  assert.deepStrictEqual(msg.toString(), data.join(''));
}));

socket.bind(() => socket.send(data, socket.address().port, 'localhost'));
