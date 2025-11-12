'use strict';

const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

const client = dgram.createSocket('udp4');

const buf = Buffer.alloc(256, 'x');

const onMessage = common.mustSucceed((bytes) => {
  assert.strictEqual(bytes, buf.length);
  client.close();
});

client.bind(0, () => client.send(buf, client.address().port, onMessage));
