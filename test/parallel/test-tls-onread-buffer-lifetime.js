// Flags: --js-immutable-arraybuffer
'use strict';

// TLS reads must not modify an immutable ArrayBuffer created by transferring a
// registered onread buffer.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const fixtures = require('../common/fixtures');
const assert = require('node:assert');
const { once } = require('node:events');
const { test } = require('node:test');
const tls = require('node:tls');

const options = {
  key: fixtures.readKey('agent2-key.pem'),
  cert: fixtures.readKey('agent2-cert.pem'),
};

for (const mode of ['before first read', 'between reads', 'replacement buffer']) {
  test(`tls onread buffer transferred ${mode}`, async (t) => {
    const backing = new ArrayBuffer(8);
    new Uint8Array(backing).fill(42);
    const buffer = mode === 'replacement buffer' ?
      Buffer.from(backing, 2, 4) : new Uint8Array(backing, 2, 4);
    const firstBuffer = new Uint8Array(1);
    const connections = new Set();
    let peer;
    let socket;
    let immutable;
    let expected;
    let factoryCalls = 0;
    let readError;
    const closed = Promise.withResolvers();

    const server = tls.createServer(options, common.mustCall((connection) => {
      peer = connection;
      connections.add(connection);
      connection.on('close', () => connections.delete(connection));
      connection.on('error', (error) => {
        if (error.code !== 'ECONNRESET') throw error;
      });
      if (mode === 'before first read') {
        connection.end('test');
      } else {
        connection.write('a');
      }
    }));
    server.on('connection', (connection) => {
      connections.add(connection);
      connection.on('close', () => connections.delete(connection));
    });
    t.after(async () => {
      socket?.destroy();
      for (const connection of connections) connection.destroy();
      if (server.listening) {
        await new Promise((resolve) => server.close(resolve));
      }
    });
    server.listen(0, '127.0.0.1');
    await once(server, 'listening');

    function transfer() {
      expected = [...new Uint8Array(backing)];
      immutable = backing.transferToImmutable();
      assert.strictEqual(buffer.byteLength, 0);
      assert.strictEqual(immutable.immutable, true);
    }

    socket = tls.connect({
      host: '127.0.0.1',
      port: server.address().port,
      rejectUnauthorized: false,
      onread: {
        buffer: mode === 'replacement buffer' ? common.mustCall(() => {
          return ++factoryCalls === 1 ? firstBuffer : buffer;
        }, 2) : buffer,
        callback: common.mustCall((nread, view) => {
          try {
            if (immutable !== undefined) {
              assert.deepStrictEqual([...new Uint8Array(immutable)], expected);
              assert.fail('onread called after the buffer was transferred');
            }
            assert.strictEqual(nread, 1);
            assert.strictEqual(view, mode === 'replacement buffer' ? firstBuffer : buffer);
            assert.strictEqual(view[0], 97);
            if (mode === 'replacement buffer') {
              // The factory's next view is registered after this callback.
              setImmediate(common.mustCall(() => {
                transfer();
                peer.end('test');
              }));
            } else {
              transfer();
              peer.end('test');
            }
          } catch (error) {
            closed.reject(error);
            return false;
          }
        }, mode === 'before first read' ? 0 : 1),
      },
    });
    socket.on('data', common.mustNotCall());
    socket.on('end', common.mustNotCall());
    socket.on('error', common.mustCall((error) => {
      readError = error;
    }));
    socket.on('close', common.mustCall(closed.resolve));
    if (mode === 'before first read') transfer();
    assert.strictEqual(await closed.promise, true);
    assert.strictEqual(readError.code, 'EINVAL');
    assert.strictEqual(readError.syscall, 'read');
    assert.deepStrictEqual([...new Uint8Array(immutable)], expected);
  });
}
