'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const net = require('net');
const tls = require('tls');

const server = net.createServer((c) => {
  c.end();
}).listen(common.mustCall(() => {
  const port = server.address().port;

  tls.connect(port, common.localhostIPv4)
    .once('error', common.mustCall((e) => {
      assert.strictEqual(e.code, 'ECONNRESET');
      assert.strictEqual(e.path, undefined);
      assert.strictEqual(e.host, common.localhostIPv4);
      assert.strictEqual(e.port, port);
      assert.strictEqual(e.localAddress, undefined);
      server.close();
    }));
}));
