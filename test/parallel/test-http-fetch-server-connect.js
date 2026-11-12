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

const kFallThrough = Symbol.for('wintertc.server.fallthrough');

async function echoTunnel(tunnel) {
  for await (const batch of tunnel.readable) {
    await tunnel.writer.writev(batch);
  }
  await tunnel.writer.end();
}

const handler = {
  fetch: common.mustCallAtLeast((ctx) => {
    return new ServerResponse(`fetch ${ctx.request.headers.get('upgrade')}`);
  }, 0),
  connect: common.mustCallAtLeast(async (ctx) => {
    const { request } = ctx;
    const proto = ctx.connectProtocol;
    if (proto === 'fall') {
      ctx.deny(kFallThrough);
      assert.throws(() => ctx.deny(), { name: 'InvalidStateError' });
      return;
    }
    if (proto === 'refuse') {
      return new ServerResponse('refused', { status: 403 });
    }
    if (proto === 'ok200') {
      // A 2xx response is valid on an upgrade-originated exchange.
      return new ServerResponse('declined');
    }
    if (proto === 'connect-udp') {
      const tunnel = await ctx.accept();
      const capsules = tunnel.capsules();
      assert.throws(() => tunnel.readable, { name: 'InvalidStateError' });
      assert.throws(() => tunnel.capsules(), { name: 'InvalidStateError' });
      const { readable, writer, unreliable } = tunnel.datagrams();
      assert.strictEqual(unreliable, false);
      (async () => {
        for await (const batch of readable) {
          for (const datagram of batch) await writer.write(datagram);
        }
      })().then(common.mustCall());
      for await (const capsule of capsules.readable) {
        await capsules.send({ type: capsule.type + 1, data: capsule.data });
      }
      await tunnel.close();
      return;
    }
    if (proto === null) {
      // Plain CONNECT.
      assert.match(request.url, /^http:\/\/(example\.com|denied):\d+\/$/);
      await assert.rejects(request.trailers, { name: 'InvalidStateError' });
      await assert.rejects(request[Symbol.asyncIterator]().next(),
                           { name: 'InvalidStateError' });
      if (request.headers.get('host') === 'denied') {
        ctx.deny();
        return;
      }
      if (request.url.startsWith('http://example.com:8443')) {
        return new ServerResponse(null, { status: 200 });
      }
    }
    const tunnel = await ctx.accept({ headers: { 'x-tunnel': 'yes' } });
    if (proto === null) {
      assert.throws(() => tunnel.capsules(), { name: 'InvalidStateError' });
      assert.throws(() => tunnel.datagrams(), { name: 'InvalidStateError' });
    }
    await assert.rejects(ctx.accept(), { name: 'InvalidStateError' });
    await echoTunnel(tunnel);
    await tunnel.closed;
  }),
};

async function start() {
  const server = serve(handler, { port: 0, hostname: '127.0.0.1' });
  await server.ready;
  return { server, port: [...server][0].address.port };
}

function readUntil(socket, pattern) {
  return new Promise((resolve) => {
    let data = '';
    const onData = (chunk) => {
      data += chunk.toString('latin1');
      if (pattern.test(data)) {
        socket.removeListener('data', onData);
        resolve(data);
      }
    };
    socket.on('data', onData);
  });
}

function encodeCapsule(type, data) {
  return Buffer.concat([Buffer.from([type, data.length]), data]);
}

test('HTTP/1.1 CONNECT', async () => {
  const { server, port } = await start();
  try {
    const socket = net.connect(port, '127.0.0.1');
    socket.write('CONNECT example.com:443 HTTP/1.1\r\nHost: example.com:443\r\n\r\n');
    const head = await readUntil(socket, /\r\n\r\n/);
    assert.match(head, /^HTTP\/1\.1 200 /);
    assert.match(head, /x-tunnel: yes/i);
    socket.write('ping');
    assert.strictEqual(await readUntil(socket, /ping/), 'ping');
    socket.end();
    await once(socket, 'close');

    const denied = net.connect(port, '127.0.0.1');
    denied.write('CONNECT example.com:443 HTTP/1.1\r\nHost: denied\r\n\r\n');
    denied.on('data', common.mustNotCall());
    await once(denied, 'close');
  } finally {
    await server.close();
  }
});

test('HTTP/1.1 upgrade normalization and fall-through', async () => {
  const { server, port } = await start();
  try {
    const upgrade = (protocol) => new Promise((resolve, reject) => {
      const req = http.request({
        host: '127.0.0.1', port, agent: false,
        headers: { 'Connection': 'Upgrade', 'Upgrade': protocol },
      });
      req.on('upgrade', (res, socket, head) => resolve({ res, socket, head }));
      req.on('response', (res) => {
        let body = '';
        res.setEncoding('utf8');
        res.on('data', (c) => body += c);
        res.on('end', () => resolve({ res, body }));
      });
      req.on('error', reject);
      req.end();
    });

    const fall = await upgrade('fall');
    assert.strictEqual(fall.res.statusCode, 200);
    assert.strictEqual(fall.body, 'fetch fall');

    const refused = await upgrade('refuse');
    assert.strictEqual(refused.res.statusCode, 403);

    const declined = await upgrade('ok200');
    assert.strictEqual(declined.body, 'declined');

    const echo = await upgrade('Echo/1, other');
    assert.strictEqual(echo.res.statusCode, 101);
    assert.strictEqual(echo.res.headers.upgrade, 'Echo/1');
    echo.socket.write('hello');
    assert.strictEqual(await readUntil(echo.socket, /hello/), 'hello');
    echo.socket.end();
  } finally {
    await server.close();
  }
});

test('HTTP/2 CONNECT and extended CONNECT', async () => {
  const { server, port } = await start();
  const client = http2.connect(`http://127.0.0.1:${port}`);
  try {
    await once(client, 'remoteSettings');
    assert.strictEqual(client.remoteSettings.enableConnectProtocol, true);

    const plain = client.request({
      ':method': 'CONNECT', ':authority': 'example.com:443',
    });
    const [headers] = await once(plain, 'response');
    assert.strictEqual(headers[':status'], 200);
    assert.strictEqual(headers['x-tunnel'], 'yes');
    plain.write('abc');
    plain.end();
    let data = '';
    plain.setEncoding('utf8');
    for await (const chunk of plain) data += chunk;
    assert.strictEqual(data, 'abc');

    // A 2xx ServerResponse cannot establish a tunnel: 502.
    const invalid = client.request({
      ':method': 'CONNECT', ':authority': 'example.com:8443',
    });
    // The server refuses the rest of the request with RST_STREAM(NO_ERROR),
    // which can arrive together with the response headers.
    invalid.on('error', () => {});
    const invalidHeaders = await new Promise((resolve) => {
      invalid.on('response', resolve);
    });
    assert.strictEqual(invalidHeaders[':status'], 502);
    invalid.resume();
    if (!invalid.closed) await once(invalid, 'close');

    const udp = client.request({
      ':method': 'CONNECT', ':protocol': 'connect-udp', ':scheme': 'https',
      ':path': '/.well-known/masque/udp/192.0.2.1/443/',
      ':authority': 'example.com',
    });
    const [udpHeaders] = await once(udp, 'response');
    assert.strictEqual(udpHeaders[':status'], 200);
    assert.strictEqual(udpHeaders['capsule-protocol'], '?1');
    udp.write(encodeCapsule(0x20, Buffer.from('cap')));
    udp.write(encodeCapsule(0x00, Buffer.from('\x00dgram')));
    udp.end();
    const received = [];
    for await (const chunk of udp) received.push(chunk);
    const all = Buffer.concat(received);
    // The capsule is echoed with type + 1, and the datagram as a datagram.
    assert.ok(all.includes(encodeCapsule(0x21, Buffer.from('cap'))));
    assert.ok(all.includes(encodeCapsule(0x00, Buffer.from('\x00dgram'))));
  } finally {
    client.close();
    await server.close();
  }
});

test('CONNECT without a connect handler', async () => {
  const server = serve({
    fetch: common.mustCall((ctx) => {
      return new ServerResponse(`fetched ${ctx.request.headers.get('upgrade')}`);
    }),
  }, { port: 0, hostname: '127.0.0.1' });
  await server.ready;
  const port = [...server][0].address.port;
  const socket = net.connect(port, '127.0.0.1');
  socket.write('CONNECT example.com:443 HTTP/1.1\r\nHost: example.com\r\n\r\n');
  assert.match(await readUntil(socket, /\r\n\r\n/), /^HTTP\/1\.1 501 /);
  socket.destroy();

  // Without connect(), an upgrade offer is ignored and routed to fetch().
  const body = await new Promise((resolve, reject) => {
    const req = http.request({
      host: '127.0.0.1', port, agent: false,
      headers: { 'Connection': 'Upgrade', 'Upgrade': 'websocket' },
    }, common.mustCall((res) => {
      assert.strictEqual(res.statusCode, 200);
      let data = '';
      res.setEncoding('utf8');
      res.on('data', (c) => data += c);
      res.on('end', () => resolve(data));
    }));
    req.on('upgrade', common.mustNotCall());
    req.on('error', reject);
    req.end();
  });
  assert.strictEqual(body, 'fetched websocket');
  await server.close();
});
