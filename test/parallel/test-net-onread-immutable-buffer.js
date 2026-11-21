// Flags: --js-immutable-arraybuffer
'use strict';

const common = require('../common');
const assert = require('assert');
const net = require('net');
const { once } = require('events');
const { test } = require('node:test');

if (typeof ArrayBuffer.prototype.transferToImmutable !== 'function')
  common.skip('ArrayBuffer.prototype.transferToImmutable is not available');

const message = Buffer.from('test');

async function createServer(t) {
  const connections = [];
  const server = net.createServer((socket) => {
    connections.push(socket);
    // Rejecting a replacement buffer may close the client with unread data.
    socket.on('error', (err) => {
      if (err.code !== 'ECONNRESET') throw err;
    });
    socket.end(message);
  });
  t.after(async () => {
    for (const socket of connections) socket.destroy();
    await new Promise((resolve) => server.close(resolve));
  });
  server.listen(0, '127.0.0.1');
  await once(server, 'listening');
  return server.address().port;
}

for (const View of [Buffer, Uint8Array]) {
  for (const offset of [0, 1]) {
    for (const mode of ['direct', 'first', 'replacement']) {
      test(`${View.name}, offset ${offset}, ${mode} immutable buffer`, {
        timeout: common.platformTimeout(5000),
      }, async (t) => {
        const original = [0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff];
        const backing = Uint8Array.from(original).buffer.transferToImmutable();
        const target = View === Buffer ?
          Buffer.from(backing, offset, 4) : new Uint8Array(backing, offset, 4);
        const mutable = new Uint8Array(1);
        let generated = 0;
        const factory = () => {
          generated++;
          return mode === 'replacement' && generated === 1 ? mutable : target;
        };
        const reads = [];
        const errors = [];
        const socket = new net.Socket({
          onread: {
            buffer: mode === 'direct' ? target : factory,
            callback(nread, buffer) {
              reads.push({ buffer, bytes: [...buffer.subarray(0, nread)] });
            },
          },
        });
        t.after(() => socket.destroy());
        socket.on('error', (err) => errors.push(err));
        socket.on('close', common.mustCall());
        socket.on('data', common.mustNotCall());
        const closed = new Promise((resolve) => socket.once('close', resolve));
        const port = await createServer(t);
        socket.connect(port, '127.0.0.1');
        await closed;

        assert.strictEqual(errors.length, 1);
        assert.strictEqual(errors[0].name, 'TypeError');
        assert.strictEqual(errors[0].code, 'ERR_INVALID_ARG_VALUE');
        assert.match(errors[0].message, /onread\.buffer.*immutable ArrayBuffer/);
        assert.strictEqual(socket.connecting, false);
        assert.strictEqual(backing.immutable, true);
        assert.deepStrictEqual([...new Uint8Array(backing)], original);
        if (mode === 'replacement') {
          assert.strictEqual(generated, 2);
          assert.strictEqual(reads.length, 1);
          assert.strictEqual(reads[0].buffer, mutable);
          assert.deepStrictEqual(reads[0].bytes, [...message.subarray(0, 1)]);
        } else {
          assert.strictEqual(generated, mode === 'first' ? 1 : 0);
          assert.deepStrictEqual(reads, []);
        }
      });
    }
  }
}

for (const kind of ['bound', 'retry']) {
  test(`reject an immutable buffer during ${kind} initialization`, {
    timeout: common.platformTimeout(5000),
  }, async (t) => {
    const port = await createServer(t);
    const backing = new ArrayBuffer(4).transferToImmutable();
    const target = new Uint8Array(backing);
    let generated = 0;
    const options = {
      host: '127.0.0.1',
      port,
      onread: {
        buffer: target,
        callback: common.mustNotCall(),
      },
    };
    let socket;
    if (kind === 'bound') {
      options.handle = new net.BoundSocket({ host: '127.0.0.1' });
      socket = net.connect(options);
    } else {
      options.host = 'localhost';
      options.autoSelectFamily = true;
      options.autoSelectFamilyAttemptTimeout = common.platformTimeout(10);
      options.lookup = (host, opts, callback) => callback(null, [
        { address: '127.0.0.2', family: 4 },
        { address: '127.0.0.1', family: 4 },
      ]);
      options.onread.buffer = () => (++generated === 1 ? new Uint8Array(4) : target);
      socket = net.connect(options).unref();
    }
    t.after(() => socket.destroy());
    socket.on('close', common.mustCall());
    socket.on('error', common.expectsError({
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_VALUE',
    }));
    await new Promise((resolve) => socket.once('close', resolve));
    assert.deepStrictEqual([...target], [0, 0, 0, 0]);
    if (kind === 'retry') assert.strictEqual(generated, 2);
  });
}

for (const shared of [false, true]) {
  test(`mutable ${shared ? 'shared' : 'on-heap'} replacement buffers`, {
    timeout: common.platformTimeout(5000),
  }, async (t) => {
    const chunks = [];
    const socket = new net.Socket({
      onread: {
        buffer: () => (shared ?
          new Uint8Array(new SharedArrayBuffer(1)) : new Uint8Array(1)),
        callback(nread, buffer) {
          chunks.push(Buffer.from(buffer.subarray(0, nread)));
        },
      },
    });
    t.after(() => socket.destroy());
    socket.on('error', common.mustNotCall());
    const closed = new Promise((resolve) => socket.once('close', resolve));
    const port = await createServer(t);
    socket.connect(port, '127.0.0.1');
    await closed;
    assert.deepStrictEqual(Buffer.concat(chunks), message);
  });
}
