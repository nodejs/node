'use strict';
require('../common');
const assert = require('assert');
const dgram = require('dgram');

const message = Buffer.from('Some bytes');
const client = dgram.createSocket('udp4');
client.send(
  message,
  0,
  message.length,
  41234,
  'localhost',
  function(err, bytes) {
    assert.strictEqual(bytes, message.length);
    client.close();
  }
);
