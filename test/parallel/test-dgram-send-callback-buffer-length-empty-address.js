'use strict';

const common = require('../common');
const assert = require('assert');

const dgram = require('dgram');
const client = dgram.createSocket('udp4');

const buf = Buffer.alloc(256, 'x');
const offset = 20;
const len = buf.length - offset;

const onMessage = common.mustSucceed(function messageSent(bytes) {
  assert.notStrictEqual(bytes, buf.length);
  assert.strictEqual(bytes, buf.length - offset);
  client.close();
});

client.bind(0, () => client.send(buf, offset, len,
                                 client.address().port,
                                 onMessage));
