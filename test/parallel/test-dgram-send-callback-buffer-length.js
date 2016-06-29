'use strict';

const common = require('../common');
const assert = require('assert');

const dgram = require('dgram');
const client = dgram.createSocket('udp4');

const timer = setTimeout(function() {
  throw new Error('Timeout');
}, common.platformTimeout(200));

const buf = Buffer.allocUnsafe(256);
const offset = 20;
const len = buf.length - offset;

const messageSent = common.mustCall(function messageSent(err, bytes) {
  assert.notEqual(bytes, buf.length);
  assert.equal(bytes, buf.length - offset);
  clearTimeout(timer);
  client.close();
});

client.send(buf, offset, len, common.PORT, '127.0.0.1', messageSent);
