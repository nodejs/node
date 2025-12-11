'use strict';

const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

const client = dgram.createSocket('udp4');

const messageSent = common.mustSucceed(function messageSent(bytes) {
  assert.strictEqual(bytes, buf1.length + buf2.length);
});

const buf1 = Buffer.alloc(256, 'x');
const buf2 = Buffer.alloc(256, 'y');

client.on('listening', function() {
  const port = this.address().port;
  client.send([buf1, buf2], port, messageSent);
});

client.on('message', common.mustCall(function onMessage(buf) {
  const expected = Buffer.concat([buf1, buf2]);
  assert.ok(buf.equals(expected), 'message was received correctly');
  client.close();
}));

client.bind(0);
