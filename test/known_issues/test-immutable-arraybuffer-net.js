// Flags: --js-immutable-arraybuffer
'use strict';

const common = require('../common');
if (!common.hasCrypto) { common.skip('missing crypto'); };
const assert = require('assert');
const { once } = require('events');
const net = require('net');
const { test } = require('node:test');

// Socket reads must not write into an immutable onread buffer.
for (const factory of [false, true]) {
  const name = factory ? 'buffer factory' : 'buffer';
  test(`net.Socket onread with ${name}`, {
    timeout: common.platformTimeout(5000),
  }, async (t) => {
    const buffer = new Uint8Array(new ArrayBuffer(4).transferToImmutable());
    const sockets = [];
    const server = net.createServer((socket) => {
      sockets.push(socket);
      socket.end('test');
    });
    t.after(async () => {
      for (const socket of sockets) socket.destroy();
      if (server.listening) {
        await new Promise((resolve, reject) => {
          server.close((err) => (err ? reject(err) : resolve()));
        });
      }
    });

    server.listen(0, '127.0.0.1');
    await once(server, 'listening');

    try {
      await new Promise((resolve, reject) => {
        const socket = net.connect({
          host: '127.0.0.1',
          port: server.address().port,
          onread: {
            buffer: factory ? () => buffer : buffer,
            callback: resolve,
          },
        });
        sockets.push(socket);
        socket.on('error', reject);
        socket.on('end', () => reject(new Error('No onread callback')));
      });
    } catch (err) {
      if (!(err instanceof TypeError)) throw err;
    }
    assert.deepStrictEqual([...buffer], [0, 0, 0, 0]);
  });
}

// ALPN normalization only needs to read the supplied protocol bytes.
for (const type of ['Buffer', 'Uint8Array']) {
  for (const api of ['createServer', 'TLSSocket']) {
    test(`tls.${api} with immutable ${type} ALPNProtocols`, {
      skip: !common.hasCrypto,
    }, (t) => {
      const tls = require('tls');
      const bytes = [2, 104, 50]; // Length-prefixed "h2".
      const buffer = Uint8Array.from(bytes).buffer.transferToImmutable();
      const protocols = type === 'Buffer' ? Buffer.from(buffer) : new Uint8Array(buffer);
      const options = { ALPNProtocols: protocols };
      if (api === 'createServer') {
        // No listening socket is needed to exercise option normalization.
        tls.createServer(options);
      } else {
        const socket = new tls.TLSSocket(undefined, options);
        t.after(() => socket.destroy());
      }
      assert.deepStrictEqual([...protocols], bytes);
      assert.strictEqual(buffer.immutable, true);
    });
  }
}
