'use strict';
const common = require('../common');
if (common.isOSX)
  common.skip('because of 17894467 Apple bug');

const assert = require('assert');
const dgram = require('dgram');

const client = dgram.createSocket('udp4');

client.bind(0, common.mustCall(function() {
  const port = this.address().port;

  client.on('message', common.mustCall(function onMessage(buffer) {
    assert.strictEqual(buffer.length, 0);
    clearInterval(interval);
    client.close();
  }));

  const buf = Buffer.alloc(0);
  let interval = setInterval(function() {
    client.send(buf, 0, 0, port, '127.0.0.1', common.mustCall());
  }, 10);
}));
