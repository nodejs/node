'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const net = require('net');
const tls = require('tls');

const server = net.createServer((c) => {
  c.end();
}).listen(0, '127.0.0.1', common.mustCall(() => {
  const port = server.address().port;

  tls.connect({
    host: '127.0.0.1',
    port: port,
    localAddress: '127.0.0.1'
  }).once('error', common.mustCall((e) => {
    assert.strictEqual(e.code, 'ECONNRESET');
    assert.strictEqual(e.path, undefined);
    assert.strictEqual(e.port, port);
    assert.strictEqual(e.localAddress, '127.0.0.1');
    server.close();
  }));
}));
