'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const net = require('net');
const tls = require('tls');

const server = net.createServer((c) => {
  c.end();
}).listen(0, common.localhostIPv4, common.mustCall(() => {
  const port = server.address().port;

  tls.connect({
    host: common.localhostIPv4,
    port: port,
    localAddress: common.localhostIPv4
  }).once('error', common.mustCall((e) => {
    assert.strictEqual(e.code, 'ECONNRESET');
    assert.strictEqual(e.path, undefined);
    assert.strictEqual(e.port, port);
    assert.strictEqual(e.localAddress, common.localhostIPv4);
    server.close();
  }));
}));
