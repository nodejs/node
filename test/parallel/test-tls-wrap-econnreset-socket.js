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

  const socket = new net.Socket();

  let errored = false;
  tls.connect({ socket })
    .once('error', common.mustCall((e) => {
      assert.strictEqual(e.code, 'ECONNRESET');
      assert.strictEqual(e.path, undefined);
      assert.strictEqual(e.host, undefined);
      assert.strictEqual(e.port, undefined);
      assert.strictEqual(e.localAddress, undefined);
      errored = true;
      server.close();
    }))
    .on('close', common.mustCall(() => {
      assert.strictEqual(errored, true);
    }));

  socket.connect(port);
}));
