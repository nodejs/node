'use strict';

const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

const client = dgram.createSocket('udp4');

const buf = Buffer.allocUnsafe(256);

const onMessage = common.mustCall(function(err, bytes) {
  assert.ifError(err);
  assert.strictEqual(bytes, buf.length);
  client.close();
});

client.bind(0, () => client.send(buf,
                                 client.address().port,
                                 common.localhostIPv4,
                                 onMessage));
