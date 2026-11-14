// Flags: --js-immutable-arraybuffer
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const { once } = require('events');
const { test } = require('node:test');
const fixtures = require('../common/fixtures');

if (typeof ArrayBuffer.prototype.transferToImmutable !== 'function')
  common.skip('ArrayBuffer.prototype.transferToImmutable is not available');

for (const factory of [false, true]) {
  test(`immutable ${factory ? 'generated' : 'static'} TLS onread buffer`, {
    timeout: common.platformTimeout(5000),
  }, async (t) => {
    const backing = new ArrayBuffer(4).transferToImmutable();
    const target = new Uint8Array(backing);
    const socket = new tls.TLSSocket(undefined, {
      onread: {
        buffer: factory ? () => target : target,
        callback: common.mustNotCall(),
      },
    });
    t.after(() => socket.destroy());
    socket.on('error', common.expectsError({
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_VALUE',
    }));
    socket.on('close', common.mustCall());
    await new Promise((resolve) => socket.once('close', resolve));
    assert.deepStrictEqual([...target], [0, 0, 0, 0]);
  });
}

test('reject an immutable TLS replacement buffer', {
  timeout: common.platformTimeout(5000),
}, async (t) => {
  const backing = new ArrayBuffer(4).transferToImmutable();
  const target = new Uint8Array(backing);
  const mutable = new Uint8Array(1);
  const connections = [];
  let socket;
  const server = tls.createServer({
    key: fixtures.readKey('agent2-key.pem'),
    cert: fixtures.readKey('agent2-cert.pem'),
  }, (socket) => {
    connections.push(socket);
    socket.on('error', (err) => {
      if (err.code !== 'ECONNRESET') throw err;
    });
    socket.end('test');
  });
  t.after(async () => {
    socket?.destroy();
    for (const socket of connections) socket.destroy();
    await new Promise((resolve) => server.close(resolve));
  });
  server.listen(0, '127.0.0.1');
  await once(server, 'listening');
  let generated = 0;
  socket = tls.connect({
    host: '127.0.0.1',
    port: server.address().port,
    rejectUnauthorized: false,
    onread: {
      buffer: common.mustCall(() => (++generated === 1 ? mutable : target), 2),
      callback: common.mustCall((nread, buffer) => {
        assert.strictEqual(buffer, mutable);
        assert.strictEqual(nread, 1);
        assert.strictEqual(buffer[0], 't'.charCodeAt(0));
      }),
    },
  });
  socket.on('error', common.expectsError({ code: 'ERR_INVALID_ARG_VALUE' }));
  socket.on('close', common.mustCall());
  await new Promise((resolve) => socket.once('close', resolve));
  assert.deepStrictEqual([...target], [0, 0, 0, 0]);
});
