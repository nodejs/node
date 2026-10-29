'use strict';
const common = require('../common');

const assert = require('assert');
const dgram = require('dgram');

const client = dgram.createSocket('udp4');

client.bind(0, common.mustCall(function() {
  const port = this.address().port;
  client.connect(port, common.mustCall(() => {
    const buf = Buffer.alloc(0);
    client.send(buf, 0, 0, common.mustSucceed());
  }));

  client.on('message', common.mustCall((buffer) => {
    assert.strictEqual(buffer.length, 0);
    client.close();
  }));
}));
