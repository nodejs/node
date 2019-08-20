'use strict';

const common = require('../common');
const assert = require('assert');

const dgram = require('dgram');
const client = dgram.createSocket('udp4');

const buf = Buffer.allocUnsafe(256);
const offset = 20;
const len = buf.length - offset;

const messageSent = common.mustCall(function messageSent(err, bytes) {
  assert.ifError(err);
  assert.notStrictEqual(bytes, buf.length);
  assert.strictEqual(bytes, buf.length - offset);
  client.close();
});

client.bind(0, common.mustCall(() => {
  client.connect(client.address().port, common.mustCall(() => {
    client.send(buf, offset, len, messageSent);
  }));
}));
