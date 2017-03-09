'use strict';
const common = require('../common');
const assert = require('assert');

const dgram = require('dgram');


const client = dgram.createSocket('udp4');

const buf = new Buffer(256);
const offset = 20;

const len = buf.length - offset;


client.send(buf, offset, len, common.PORT, '127.0.0.1', function(err, bytes) {
  assert.notEqual(bytes, buf.length);
  assert.equal(bytes, buf.length - offset);
  clearTimeout(timer);
  client.close();
});

const timer = setTimeout(function() {
  throw new Error('Timeout');
}, 200);
