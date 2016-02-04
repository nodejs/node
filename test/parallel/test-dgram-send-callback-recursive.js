'use strict';
const common = require('../common');
const assert = require('assert');

const dgram = require('dgram');
const client = dgram.createSocket('udp4');
const chunk = 'abc';
let received = 0;
let sent = 0;
const limit = 10;
let async = false;

function onsend() {
  if (sent++ < limit) {
    client.send(
      chunk, 0, chunk.length, common.PORT, common.localhostIPv4, onsend);
  } else {
    assert.strictEqual(async, true, 'Send should be asynchronous.');
  }
}

client.on('listening', function() {
  setImmediate(function() {
    async = true;
  });

  onsend();
});

client.on('message', function(buf, info) {
  received++;
  if (received === limit) {
    client.close();
  }
});

client.on('close', common.mustCall(function() {
  assert.equal(received, limit);
}));

client.bind(common.PORT);
