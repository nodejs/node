// Flags: --experimental-http-serve --no-warnings
'use strict';

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const assert = require('assert');
const http = require('http');
const http2 = require('http2');
const net = require('net');
const { once } = require('events');
const { test } = require('node:test');
const { serve, ServerResponse } = http.fetchServer;

// Raw RFC 6455 client frames, masked as a client must send them.
function frame(opcode, payload = Buffer.alloc(0), { fin = true, mask = true,
                                                    rsv1 = false } = {}) {
  payload = Buffer.from(payload);
  const length = payload.length;
  const header = [(fin ? 0x80 : 0) | (rsv1 ? 0x40 : 0) | opcode];
  if (length < 126) {
    header.push((mask ? 0x80 : 0) | length);
  } else {
    header.push((mask ? 0x80 : 0) | 126, length >> 8, length & 0xff);
  }
  if (!mask) return Buffer.concat([Buffer.from(header), payload]);
  const key = Buffer.from([1, 2, 3, 4]);
  const masked = Buffer.from(payload);
  for (let i = 0; i < masked.length; i++) masked[i] ^= key[i & 3];
  return Buffer.concat([Buffer.from(header), key, masked]);
}

function closePayload(code, reason = '') {
  const buffer = Buffer.alloc(2);
  buffer.writeUInt16BE(code);
  return Buffer.concat([buffer, Buffer.from(reason)]);
}

// Parses unmasked server frames.
function parseFrames(buffer) {
  const frames = [];
  let offset = 0;
  while (offset + 2 <= buffer.length) {
    let length = buffer[offset + 1] & 0x7f;
    let start = offset + 2;
    if (length === 126) {
      length = buffer.readUInt16BE(offset + 2);
      start += 2;
    } else if (length === 127) {
      length = Number(buffer.readBigUInt64BE(offset + 2));
      start += 8;
    }
    if (start + length > buffer.length) break;
    frames.push({
      fin: (buffer[offset] & 0x80) !== 0,
      opcode: buffer[offset] & 0x0f,
      payload: buffer.subarray(start, start + length),
    });
    offset = start + length;
  }
  return { frames, rest: buffer.subarray(offset) };
}

class RawClient {
  constructor(socket, head) {
    this.socket = socket;
    this.buffer = head ?? Buffer.alloc(0);
    this.frames = [];
    this.waiters = [];
    socket.on('data', (chunk) => {
      this.buffer = Buffer.concat([this.buffer, chunk]);
      const { frames, rest } = parseFrames(this.buffer);
      this.buffer = rest;
      this.frames.push(...frames);
      this.waiters.splice(0).forEach((fn) => fn());
    });
    socket.on('end', () => this.waiters.splice(0).forEach((fn) => fn()));
  }

  async next() {
    while (this.frames.length === 0) {
      await new Promise((resolve) => this.waiters.push(resolve));
    }
    return this.frames.shift();
  }

  send(data) {
    this.socket.write(data);
  }
}

async function rawUpgrade(port, path, headers = {}) {
  const socket = net.connect(port, '127.0.0.1');
  const lines = [
    `GET ${path} HTTP/1.1`,
    'Host: 127.0.0.1',
    'Connection: Upgrade',
    'Upgrade: websocket',
    'Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==',
    'Sec-WebSocket-Version: 13',
  ];
  for (const [name, value] of Object.entries(headers)) {
    lines.push(`${name}: ${value}`);
  }
  socket.write(`${lines.join('\r\n')}\r\n\r\n`);
  return new Promise((resolve, reject) => {
    let data = Buffer.alloc(0);
    const onData = (chunk) => {
      data = Buffer.concat([data, chunk]);
      const end = data.indexOf('\r\n\r\n');
      if (end === -1) return;
      socket.removeListener('data', onData);
      const head = data.subarray(0, end).toString('latin1');
      resolve({ head, client: new RawClient(socket, data.subarray(end + 4)) });
    };
    socket.on('data', onData);
    socket.on('error', reject);
  });
}

async function start(connect, options = {}) {
  const server = serve({
    fetch() {
      return new ServerResponse('fetch');
    },
    connect,
  }, { port: 0, hostname: '127.0.0.1', ...options });
  await server.ready;
  return { server, port: [...server][0].address.port };
}

test('upgradeWebSocket() with the WHATWG WebSocket client', async () => {
  const serverClose = Promise.withResolvers();
  const { server, port } = await start(common.mustCall((ctx) => {
    const ws = ctx.upgradeWebSocket({ protocol: ['chat.v2', 'chat.v1'],
                                      compression: true });
    assert.strictEqual(ws.readyState, ws.OPEN);
    assert.strictEqual(ws.url, `ws://127.0.0.1:${port}/chat`);
    assert.strictEqual(ws.protocol, 'chat.v2');
    assert.strictEqual(ws.extensions, 'permessage-deflate');
    assert.strictEqual(ws.binaryType, 'blob');
    assert.strictEqual(Object.prototype.toString.call(ws), '[object WebSocket]');
    assert.throws(() => ctx.upgradeWebSocket(), { name: 'InvalidStateError' });
    assert.throws(() => ws.close(1005), { name: 'InvalidAccessError' });
    assert.throws(() => ws.close(1000, 'x'.repeat(124)), { name: 'SyntaxError' });
    ws.binaryType = 'arraybuffer';
    ws.onmessage = common.mustCall(async (event) => {
      if (event.data === 'bye') {
        ws.close(1001, 'going');
        assert.strictEqual(ws.readyState, ws.CLOSING);
        return;
      }
      ws.send(typeof event.data === 'string' ? event.data.toUpperCase() :
        new Uint8Array(event.data).reverse());
      ws.send(new Blob(['blob']));
    }, 4);
    ws.addEventListener('close', common.mustCall((event) => {
      assert.strictEqual(event.wasClean, true);
      assert.strictEqual(event.code, 1001);
      serverClose.resolve();
    }));
  }));
  try {
    const client = new WebSocket(`ws://127.0.0.1:${port}/chat`,
                                 ['chat.v1', 'chat.v2']);
    client.binaryType = 'arraybuffer';
    await once(client, 'open');
    assert.strictEqual(client.protocol, 'chat.v2');
    assert.strictEqual(client.extensions, 'permessage-deflate');
    const received = [];
    client.onmessage = (event) => {
      received.push(typeof event.data === 'string' ? event.data :
        Buffer.from(event.data).toString('hex'));
    };
    client.send('hello');
    client.send('z'.repeat(4096));
    client.send(new Uint8Array([1, 2, 3]));
    client.send('bye');
    const [event] = await once(client, 'close');
    assert.strictEqual(event.code, 1001);
    assert.strictEqual(event.reason, 'going');
    assert.strictEqual(event.wasClean, true);
    assert.deepStrictEqual(received, [
      'HELLO', '626c6f62', 'Z'.repeat(4096), '626c6f62', '030201', '626c6f62',
    ]);
    await serverClose.promise;
  } finally {
    await server.close();
  }
});

test('upgradeWebSocketStream() echo with backpressure', async () => {
  const closed = Promise.withResolvers();
  const { server, port } = await start(common.mustCall(async (ctx) => {
    await using wss = ctx.upgradeWebSocketStream();
    const { readable, writable, protocol, extensions } = await wss.opened;
    assert.strictEqual(protocol, '');
    assert.strictEqual(extensions, '');
    const writer = writable.getWriter();
    for await (const message of readable) {
      if (typeof message === 'string') {
        await writer.write(`echo:${message}`);
      } else {
        assert.ok(message instanceof Uint8Array);
        await writer.write(message);
      }
    }
    closed.resolve(await wss.closed);
  }));
  try {
    const client = new WebSocket(`ws://127.0.0.1:${port}/`);
    client.binaryType = 'arraybuffer';
    await once(client, 'open');
    const received = [];
    client.onmessage = (event) => {
      received.push(typeof event.data === 'string' ? event.data :
        event.data.byteLength);
      if (received.length === 3) client.close(4001, 'client done');
    };
    client.send('one');
    client.send(new Uint8Array(200000));
    client.send('two');
    const [event] = await once(client, 'close');
    assert.strictEqual(event.code, 4001);
    assert.deepStrictEqual(received, ['echo:one', 200000, 'echo:two']);
    assert.deepStrictEqual({ ...await closed.promise },
                           { closeCode: 4001, reason: 'client done' });
  } finally {
    await server.close();
  }
});

test('handshake validation', async () => {
  const { server, port } = await start(common.mustCall((ctx) => {
    if (ctx.connectProtocol !== 'websocket') {
      assert.throws(() => ctx.upgradeWebSocket(), { name: 'InvalidStateError' });
      assert.throws(() => ctx.upgradeWebSocketStream(),
                    { name: 'InvalidStateError' });
      return new ServerResponse(null, { status: 501 });
    }
    assert.throws(() => ctx.upgradeWebSocket({ pingInterval: -1 }), TypeError);
    try {
      ctx.upgradeWebSocketStream();
    } catch (err) {
      assert.strictEqual(err.name, 'TypeError');
      return new ServerResponse(null, {
        status: 426, headers: { 'sec-websocket-version': '13' },
      });
    }
  }, 2));
  try {
    const invalid = await rawUpgrade(port, '/', {
      'Sec-WebSocket-Version': '8',
    }).catch(() => null);
    // The second Sec-WebSocket-Version makes the field value "13, 8".
    assert.match(invalid.head, /^HTTP\/1\.1 426 /);
    invalid.client.socket.destroy();

    const other = net.connect(port, '127.0.0.1');
    other.write('GET / HTTP/1.1\r\nHost: x\r\nConnection: Upgrade\r\n' +
                'Upgrade: other\r\n\r\n');
    const [chunk] = await once(other, 'data');
    assert.match(chunk.toString(), /^HTTP\/1\.1 501 /);
    other.destroy();
  } finally {
    await server.close();
  }
});

test('raw protocol: fragmentation, ping, and close', async () => {
  const done = Promise.withResolvers();
  const { server, port } = await start(common.mustCall(async (ctx) => {
    const wss = ctx.upgradeWebSocketStream();
    const { readable, writable } = await wss.opened;
    const reader = readable.getReader();
    const writer = writable.getWriter();
    const first = await reader.read();
    assert.strictEqual(first.value, 'hello world');
    await writer.write(new Uint8Array([9, 8]));
    const end = await reader.read();
    assert.strictEqual(end.done, true);
    done.resolve(await wss.closed);
    await assert.rejects(writer.write('late'), { name: 'InvalidStateError' });
  }));
  try {
    const { head, client } = await rawUpgrade(port, '/');
    assert.match(head, /^HTTP\/1\.1 101 /);
    assert.match(head, /Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK\+xOo=/i);
    // A fragmented text message with a ping between the fragments.
    client.send(frame(0x1, 'hello ', { fin: false }));
    client.send(frame(0x9, 'p'));
    client.send(frame(0x0, 'world'));
    const pong = await client.next();
    assert.strictEqual(pong.opcode, 0xa);
    assert.strictEqual(pong.payload.toString(), 'p');
    const binary = await client.next();
    assert.strictEqual(binary.opcode, 0x2);
    assert.deepStrictEqual([...binary.payload], [9, 8]);
    client.send(frame(0x8, closePayload(4002, 'bye')));
    const close = await client.next();
    assert.strictEqual(close.opcode, 0x8);
    assert.strictEqual(close.payload.readUInt16BE(), 4002);
    await once(client.socket, 'end');
    assert.deepStrictEqual({ ...await done.promise },
                           { closeCode: 4002, reason: 'bye' });
    client.socket.destroy();
  } finally {
    await server.close();
  }
});

test('protocol violations fail the connection', async () => {
  const cases = [
    [frame(0x1, 'x', { mask: false }), 1002],
    [frame(0x3, 'x'), 1002],
    [frame(0x0, 'x'), 1002],
    [frame(0x1, Buffer.from([0xff, 0xfe])), 1007],
    [frame(0x1, 'x', { rsv1: true }), 1002],
    [frame(0x8, Buffer.from([0x03, 0xed])), 1002],
  ];
  let index = 0;
  const { server, port } = await start(common.mustCall(async (ctx) => {
    const wss = ctx.upgradeWebSocketStream();
    const { readable } = await wss.opened;
    ctx.completed.catch(() => {});
    await assert.rejects(readable.getReader().read(),
                         { code: 'ERR_HTTP_CONNECT_ERROR' });
    await assert.rejects(wss.closed, { code: 'ERR_HTTP_CONNECT_ERROR' });
  }, cases.length));
  try {
    for (; index < cases.length; index++) {
      const [data, code] = cases[index];
      const { client } = await rawUpgrade(port, '/');
      client.send(data);
      const close = await client.next();
      assert.strictEqual(close.opcode, 0x8);
      assert.strictEqual(close.payload.readUInt16BE(), code);
      client.socket.destroy();
    }
  } finally {
    await server.close();
  }
});

test('pingInterval keeps the connection alive', async () => {
  const { server, port } = await start(common.mustCall(async (ctx) => {
    const wss = ctx.upgradeWebSocketStream({ pingInterval: 20 });
    await wss.closed;
  }));
  try {
    const { client } = await rawUpgrade(port, '/');
    const ping = await client.next();
    assert.strictEqual(ping.opcode, 0x9);
    client.send(frame(0x8));
    const close = await client.next();
    assert.strictEqual(close.opcode, 0x8);
    assert.strictEqual(close.payload.length, 0);
    client.socket.destroy();
  } finally {
    await server.close();
  }
});

test('destroy() and peer loss', async () => {
  const lost = Promise.withResolvers();
  const { server, port } = await start(common.mustCall(async (ctx) => {
    const wss = ctx.upgradeWebSocketStream();
    await wss.opened;
    if (new URL(ctx.request.url).pathname === '/destroy') {
      wss.destroy();
      await assert.rejects(wss.closed, { code: 'ERR_HTTP_REQUEST_CANCELLED' });
      wss.destroy();
      return;
    }
    await assert.rejects(ctx.completed, { code: 'ERR_HTTP_CONNECTION_RESET' });
    lost.resolve(await wss.closed.catch((err) => err.code));
  }, 2));
  try {
    const client = new WebSocket(`ws://127.0.0.1:${port}/destroy`);
    const [event] = await once(client, 'close');
    assert.strictEqual(event.code, 1006);
    assert.strictEqual(event.wasClean, false);

    const raw = await rawUpgrade(port, '/lost');
    // A TCP reset is a lost connection.
    raw.client.socket.resetAndDestroy();
    assert.strictEqual(await lost.promise, 'ERR_HTTP_CONNECTION_RESET');
  } finally {
    await server.close();
  }
});

test('server close() sends 1001 Going Away', async () => {
  const { server, port } = await start(common.mustCall((ctx) => {
    ctx.upgradeWebSocket();
  }));
  const client = new WebSocket(`ws://127.0.0.1:${port}/`);
  await once(client, 'open');
  const closing = server.close();
  const [event] = await once(client, 'close');
  assert.strictEqual(event.code, 1001);
  assert.strictEqual(event.wasClean, true);
  await closing;
});

test('WebSocket over HTTP/2 extended CONNECT', async () => {
  const { server, port } = await start(common.mustCall(async (ctx) => {
    assert.strictEqual(ctx.connectProtocol, 'websocket');
    const wss = ctx.upgradeWebSocketStream({ protocol: ['p1'] });
    const { readable, writable, protocol } = await wss.opened;
    assert.strictEqual(protocol, 'p1');
    const writer = writable.getWriter();
    for await (const message of readable) await writer.write(message);
    await wss.closed;
  }));
  const session = http2.connect(`http://127.0.0.1:${port}`);
  try {
    await once(session, 'remoteSettings');
    const stream = session.request({
      ':method': 'CONNECT', ':protocol': 'websocket', ':scheme': 'http',
      ':path': '/', ':authority': `127.0.0.1:${port}`,
      'sec-websocket-version': '13', 'sec-websocket-protocol': 'p0, p1',
    });
    const [headers] = await once(stream, 'response');
    assert.strictEqual(headers[':status'], 200);
    assert.strictEqual(headers['sec-websocket-protocol'], 'p1');
    assert.strictEqual(headers['sec-websocket-accept'], undefined);
    const client = new RawClient(stream);
    client.send(frame(0x1, 'over h2'));
    const echo = await client.next();
    assert.strictEqual(echo.payload.toString(), 'over h2');
    client.send(frame(0x8, closePayload(1000)));
    const close = await client.next();
    assert.strictEqual(close.opcode, 0x8);
    stream.end();
    await once(stream, 'close');
  } finally {
    session.close();
    await server.close();
  }
});
