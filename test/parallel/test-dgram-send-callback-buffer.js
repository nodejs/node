'use strict';

const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

const client = dgram.createSocket('udp4');

const buf = Buffer.allocUnsafe(256);

const onMessage = common.mustCall(function(err, bytes) {
  assert.strictEqual(err, null);
  assert.equal(bytes, buf.length);
  clearTimeout(timer);
  client.close();
});

const timer = setTimeout(function() {
  throw new Error('Timeout');
}, common.platformTimeout(200));

client.send(buf, common.PORT, common.localhostIPv4, onMessage);
