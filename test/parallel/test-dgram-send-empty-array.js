'use strict';

const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

if (common.isOSX) {
  common.skip('because of 17894467 Apple bug');
  return;
}

const client = dgram.createSocket('udp4');

const timer = setTimeout(function() {
  throw new Error('Timeout');
}, common.platformTimeout(200));

client.on('message', common.mustCall(function onMessage(buf, info) {
  const expected = Buffer.alloc(0);
  assert.ok(buf.equals(expected), 'message was received correctly');
  clearTimeout(timer);
  client.close();
}));

client.on('listening', function() {
  client.send([], this.address().port, common.localhostIPv4);
});

client.bind(0);
