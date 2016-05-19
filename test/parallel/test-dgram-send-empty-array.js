'use strict';

const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

if (process.platform === 'darwin') {
  common.skip('because of 17894467 Apple bug');
  return;
}

const client = dgram.createSocket('udp4');

const timer = setTimeout(function() {
  throw new Error('Timeout');
}, common.platformTimeout(200));

const onMessage = common.mustCall(function(err, bytes) {
  assert.equal(bytes, 0);
  clearTimeout(timer);
  client.close();
});

client.on('listening', function() {
  const toSend = [];
  client.send(toSend, common.PORT, common.localhostIPv4, onMessage);
});

client.on('message', function(buf, info) {
  const expected = Buffer.alloc(0);
  assert.ok(buf.equals(expected), 'message was received correctly');
  client.close();
});

client.bind(common.PORT);
