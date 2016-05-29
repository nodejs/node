'use strict';

const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

const client = dgram.createSocket('udp4');

const timer = setTimeout(function() {
  throw new Error('Timeout');
}, common.platformTimeout(200));

const onMessage = common.mustCall(function(err, bytes) {
  assert.equal(bytes, buf1.length + buf2.length);
  clearTimeout(timer);
});

const buf1 = Buffer.alloc(256, 'x');
const buf2 = Buffer.alloc(256, 'y');

client.on('listening', function() {
  const toSend = [buf1, buf2];
  client.send(toSend, this.address().port, common.localhostIPv4, onMessage);
  toSend.splice(0, 2);
});

client.on('message', common.mustCall(function onMessage(buf, info) {
  const expected = Buffer.concat([buf1, buf2]);
  assert.ok(buf.equals(expected), 'message was received correctly');
  client.close();
}));

client.bind(0);
