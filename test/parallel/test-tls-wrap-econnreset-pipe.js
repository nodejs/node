'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const net = require('net');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const server = net.createServer((c) => {
  c.end();
}).listen(common.PIPE, common.mustCall(() => {
  tls.connect({ path: common.PIPE })
    .once('error', common.mustCall((e) => {
      assert.strictEqual(e.code, 'ECONNRESET');
      assert.strictEqual(e.path, common.PIPE);
      assert.strictEqual(e.port, undefined);
      assert.strictEqual(e.host, undefined);
      assert.strictEqual(e.localAddress, undefined);
      server.close();
    }));
}));
