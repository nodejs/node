'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const net = require('net');
const fixtures = require('../common/fixtures');

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
};

const server = tls.createServer(options, common.mustCall((c) => {
  setImmediate(() => {
    c.write('hello', () => {
      setImmediate(() => {
        c.destroy();
        server.close();
      });
    });
  });
}));

let socket;
let lastIdleStart;

server.listen(0, () => {
  socket = net.connect(server.address().port, function() {
    const s = socket.setTimeout(Number.MAX_VALUE, function() {
      throw new Error('timeout');
    });
    assert.ok(s instanceof net.Socket);

    assert.notStrictEqual(socket._idleTimeout, -1);
    lastIdleStart = socket._idleStart;

    const tsocket = tls.connect({
      socket: socket,
      rejectUnauthorized: false
    });
    tsocket.resume();
  });
});

process.on('exit', () => {
  assert.strictEqual(socket._idleTimeout, -1);
  assert(lastIdleStart < socket._idleStart);
});
