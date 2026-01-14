// Flags: --permission --allow-fs-read=*
'use strict';

const common = require('../common');
if (!common.hasCrypto) { common.skip('missing crypto'); };

if (common.isWindows) {
  common.skip('This test only works on unix');
}

const assert = require('assert');
const net = require('net');
const tls = require('tls');

{
  const client = net.connect({ path: '/tmp/perm.sock' });
  client.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_ACCESS_DENIED');
  }));

  client.on('connect', common.mustNotCall('TCP connection should be blocked'));
}

{
  const client = tls.connect({ path: '/tmp/perm.sock' });
  client.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_ACCESS_DENIED');
  }));

  client.on('connect', common.mustNotCall('TCP connection should be blocked'));
}
