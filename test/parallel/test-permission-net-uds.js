// Flags: --permission --allow-fs-read=*
'use strict';

const common = require('../common');
if (!common.hasCrypto) { common.skip('missing crypto'); };

if (common.isWindows) {
  common.skip('This test only works on unix');
}

const assert = require('assert');
const fs = require('fs');
const net = require('net');
const tls = require('tls');

const pipePath = (name) => `/tmp/node-test-${process.pid}-${name}.sock`;

assert.strictEqual(process.permission.has('net'), false);

{
  const client = net.connect({ path: pipePath('perm') });
  client.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_ACCESS_DENIED');
  }));

  client.on('connect', common.mustNotCall('TCP connection should be blocked'));
}

{
  const client = tls.connect({ path: pipePath('perm-tls') });
  client.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_ACCESS_DENIED');
  }));

  client.on('connect', common.mustNotCall('TCP connection should be blocked'));
}

{
  const path = pipePath('perm-server');
  const server = net.createServer();
  assert.throws(() => {
    server.listen(path);
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'Net',
  });
  assert.strictEqual(fs.existsSync(path), false);
}

{
  const path = pipePath('perm-tls-server');
  const server = tls.createServer();
  assert.throws(() => {
    server.listen(path);
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'Net',
  });
  assert.strictEqual(fs.existsSync(path), false);
}
