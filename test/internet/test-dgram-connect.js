'use strict';

const common = require('../common');
const { addresses } = require('../common/internet');
const assert = require('assert');
const dgram = require('dgram');

const client = dgram.createSocket('udp4');
client.connect(common.PORT, addresses.INVALID_HOST, common.mustCall((err) => {
  assert.ok(err.code === 'ENOTFOUND' || err.code === 'EAI_AGAIN');

  client.once('error', common.mustCall((err) => {
    assert.ok(err.code === 'ENOTFOUND' || err.code === 'EAI_AGAIN');
    client.once('connect', common.mustCall(() => client.close()));
    client.connect(common.PORT);
  }));

  client.connect(common.PORT, addresses.INVALID_HOST);
}));
