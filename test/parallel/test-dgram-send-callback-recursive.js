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
let port;

function onsend() {
  if (sent++ < limit) {
    client.send(chunk, 0, chunk.length, port, common.localhostIPv4, onsend);
  } else {
    assert.strictEqual(async, true);
  }
}

client.on('listening', function() {
  port = this.address().port;

  process.nextTick(() => {
    async = true;
  });

  onsend();
});

client.on('message', (buf, info) => {
  received++;
  if (received === limit) {
    client.close();
  }
});

client.on('close', common.mustCall(function() {
  assert.strictEqual(received, limit);
}));

client.bind(0);
