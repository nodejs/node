'use strict';

const common = require('../common');

if (common.isOSX) {
  common.skip('because of 17894467 Apple bug');
  return;
}

const assert = require('assert');
const dgram = require('dgram');

const client = dgram.createSocket('udp4');

var interval;

client.on('message', common.mustCall(function onMessage(buf, info) {
  const expected = Buffer.alloc(0);
  assert.ok(buf.equals(expected), 'message was received correctly');
  clearInterval(interval);
  client.close();
}));

client.on('listening', common.mustCall(function() {
  interval = setInterval(function() {
    client.send([], client.address().port, common.localhostIPv4);
  }, 10);
}));

client.bind(0);
