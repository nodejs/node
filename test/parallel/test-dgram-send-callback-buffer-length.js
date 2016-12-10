'use strict';

const common = require('../common');
const assert = require('assert');

const dgram = require('dgram');
const client = dgram.createSocket('udp4');

const buf = Buffer.allocUnsafe(256);
const offset = 20;
const len = buf.length - offset;

const messageSent = common.mustCall(function messageSent(err, bytes) {
  assert.notStrictEqual(bytes, buf.length);
  assert.strictEqual(bytes, buf.length - offset);
  client.close();
});

client.send(buf, offset, len, common.PORT, '127.0.0.1', messageSent);
