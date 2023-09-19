'use strict';

const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

const client = dgram.createSocket('udp4');

const onMessage = common.mustCall(common.mustSucceed((bytes) => {
  assert.strictEqual(bytes, buf1.length + buf2.length);
}));

const buf1 = Buffer.alloc(256, 'x');
const buf2 = Buffer.alloc(256, 'y');

client.on('listening', common.mustCall(function() {
  const toSend = [buf1, buf2];
  client.connect(client.address().port, common.mustCall(() => {
    client.send(toSend, onMessage);
  }));
}));

client.on('message', common.mustCall(function onMessage(buf, info) {
  const expected = Buffer.concat([buf1, buf2]);
  assert.ok(buf.equals(expected), 'message was received correctly');
  client.close();
}));

client.bind(0);
