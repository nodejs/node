// Flags: --experimental-http-serve --no-warnings
'use strict';

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const assert = require('assert');
const http = require('http');
const http2 = require('http2');
const net = require('net');
const fixtures = require('../common/fixtures');
const { test } = require('node:test');
const { serve, ServerResponse } = http.fetchServer;

const key = fixtures.readKey('agent1-key.pem');
const cert = fixtures.readKey('agent1-cert.pem');
const ca = fixtures.readKey('ca1-cert.pem');

const handler = {
  [Symbol.for('wintertc.server.protocol')]: 1,
  fetch: common.mustCallAtLeast(async function fetch(ctx) {
    const { request } = ctx;
    const url = new URL(request.url);
    switch (url.pathname) {
      case '/info':
        return ServerResponse.json({
          method: request.method,
          url: request.url,
          alpn: ctx.alpnProtocol,
          encrypted: ctx.encrypted,
          family: ctx.remoteAddress.family,
          localPort: ctx.localAddress.port,
          host: request.headers.get('host'),
          priority: ctx.clientPriority,
          thisIsHandler: this === handler,
        });
      case '/echo': {
        const chunks = [];
        for await (const batch of request) chunks.push(...batch);
        const trailers = await request.trailers;
        return new ServerResponse(Buffer.concat(chunks), {
          headers: { 'x-trailer': trailers.get('x-t') ?? '' },
        });
      }
      case '/trailers': {
        const done = Promise.withResolvers();
        async function* body() {
          yield [Buffer.from('data')];
          done.resolve({ 'x-sum': '4', 'content-type': 'dropped' });
        }
        return new ServerResponse(body(), {
          headers: { 'Trailer': 'x-sum, content-type' },
          trailers: done.promise,
        });
      }
      case '/informational':
        ctx.sendInformational(103, { link: '</style.css>; rel=preload' });
        assert.throws(() => ctx.sendInformational(101), RangeError);
        assert.throws(() => ctx.sendInformational(100),
                      { name: 'InvalidStateError' });
        assert.throws(() => ctx.deny(), { name: 'InvalidStateError' });
        return new ServerResponse('after hints');
      case '/deny':
        ctx.deny();
        assert.throws(() => ctx.deny(), { name: 'InvalidStateError' });
        return;
      case '/throw':
        throw new Error('boom');
      case '/invalid':
        return 'not a response';
      case '/error-response':
        return ServerResponse.error();
      case '/wait-until': {
        const { waitUntil } = ctx;
        waitUntil(new Promise((resolve) => setTimeout(resolve, 10)));
        return new ServerResponse('ok');
      }
      case '/completed':
        ctx.completed.then(common.mustCall());
        return new ServerResponse('ok');
      case '/middleware': {
        const inner = Object.create(ctx, {
          request: { value: { url: 'http://rewritten/' } },
        });
        assert.strictEqual(inner.request.url, 'http://rewritten/');
        assert.strictEqual(typeof inner.waitUntil, 'function');
        return new ServerResponse(inner.alpnProtocol);
      }
      case '/fetch-response':
        return new Response('from Response', { status: 202 });
      case '/head':
        return new ServerResponse('body', { headers: { 'content-length': '4' } });
      default:
        return new ServerResponse('not found', { status: 404 });
    }
  }),
  error(ctx, err) {
    if (err.message === 'boom') {
      return new ServerResponse('handled', { status: 599 });
    }
  },
};

async function startServer(options = {}) {
  const server = serve(handler, { port: 0, hostname: '127.0.0.1', ...options });
  await server.ready;
  const [listener] = server;
  return { server, port: listener.address.port };
}

function h1Request(port, path, { method = 'GET', body, headers, tls } = {}) {
  return new Promise((resolve, reject) => {
    const mod = tls ? require('https') : http;
    const req = mod.request({
      host: '127.0.0.1', port, path, method, headers, ca, servername: 'agent1',
      agent: false,
    }, (res) => {
      const chunks = [];
      res.on('data', (c) => chunks.push(c));
      res.on('end', () => resolve({
        status: res.statusCode,
        headers: res.headers,
        trailers: res.trailers,
        body: Buffer.concat(chunks).toString(),
      }));
    });
    req.on('error', reject);
    req.on('information', (info) => {
      (req.info ??= []).push(info.statusCode);
    });
    if (body !== undefined) {
      req.addTrailers?.({});
      req.end(body);
    } else {
      req.end();
    }
  });
}

function h2Request(client, path, { method = 'GET', body } = {}) {
  return new Promise((resolve, reject) => {
    const stream = client.request({ ':path': path, ':method': method },
                                  { waitForTrailers: false });
    const result = { info: [], trailers: null };
    stream.on('headers', (headers) => result.info.push(headers[':status']));
    stream.on('response', (headers) => { result.headers = headers; });
    stream.on('trailers', (trailers) => { result.trailers = trailers; });
    const chunks = [];
    stream.on('data', (c) => chunks.push(c));
    stream.on('end', () => {
      result.status = result.headers[':status'];
      result.body = Buffer.concat(chunks).toString();
      resolve(result);
    });
    stream.on('error', () => {});
    stream.on('close', () => {
      if (stream.rstCode) {
        reject(Object.assign(new Error('reset'), { rstCode: stream.rstCode }));
      }
    });
    stream.end(body);
  });
}

test('handler conversion', () => {
  assert.throws(() => serve({}), TypeError);
  assert.throws(() => serve(null), TypeError);
  assert.throws(() => serve({ fetch: 1 }), TypeError);
  assert.throws(() => serve({
    [Symbol.for('wintertc.server.protocol')]: '1', fetch() {},
  }), TypeError);
  assert.throws(() => serve({ fetch() {} }, { port: 0, quic: true }), TypeError);
  assert.throws(() => serve({ fetch() {} }, {
    port: 0, quic: true, tls: { key, cert },
  }), { name: 'NotSupportedError' });
});

test('HTTP/1.1', async () => {
  const { server, port } = await startServer();
  try {
    const info = JSON.parse((await h1Request(port, '/info?a=1', {
      headers: { priority: 'u=1, i' },
    })).body);
    assert.deepStrictEqual(info, {
      method: 'GET',
      url: `http://127.0.0.1:${port}/info?a=1`,
      alpn: 'http/1.1',
      encrypted: false,
      family: 'IPv4',
      localPort: port,
      host: `127.0.0.1:${port}`,
      priority: { urgency: 1, incremental: true },
      thisIsHandler: true,
    });

    const echo = await h1Request(port, '/echo', {
      method: 'POST', body: 'x'.repeat(65536 * 3),
    });
    assert.strictEqual(echo.body.length, 65536 * 3);

    const trailers = await h1Request(port, '/trailers');
    assert.strictEqual(trailers.body, 'data');
    assert.deepStrictEqual({ ...trailers.trailers }, { 'x-sum': '4' });

    const informational = await h1Request(port, '/informational');
    assert.strictEqual(informational.body, 'after hints');

    assert.strictEqual((await h1Request(port, '/throw')).status, 599);
    assert.strictEqual((await h1Request(port, '/invalid')).status, 500);
    await assert.rejects(h1Request(port, '/deny'), { code: 'ECONNRESET' });
    await assert.rejects(h1Request(port, '/error-response'),
                         { code: 'ECONNRESET' });
    assert.strictEqual((await h1Request(port, '/wait-until')).body, 'ok');
    assert.strictEqual((await h1Request(port, '/completed')).body, 'ok');
    assert.strictEqual((await h1Request(port, '/middleware')).body, 'http/1.1');
    const fromResponse = await h1Request(port, '/fetch-response');
    assert.strictEqual(fromResponse.status, 202);
    assert.strictEqual(fromResponse.body, 'from Response');
    const head = await h1Request(port, '/head', { method: 'HEAD' });
    assert.strictEqual(head.headers['content-length'], '4');
    assert.strictEqual(head.body, '');
    assert.strictEqual((await fetch(`http://127.0.0.1:${port}/nope`)).status,
                       404);
  } finally {
    await server.close();
  }
});

test('Expect: 100-continue', async () => {
  const { server, port } = await startServer();
  try {
    const result = await new Promise((resolve, reject) => {
      const req = http.request({
        host: '127.0.0.1', port, path: '/echo', method: 'POST',
        headers: { 'expect': '100-continue', 'content-length': 5 },
      });
      req.on('continue', () => req.end('hello'));
      req.on('response', (res) => {
        let body = '';
        res.setEncoding('utf8');
        res.on('data', (c) => body += c);
        res.on('end', () => resolve(body));
      });
      req.on('error', reject);
    });
    assert.strictEqual(result, 'hello');
  } finally {
    await server.close();
  }
});

test('h2c with prior knowledge', async () => {
  const { server, port } = await startServer();
  const client = http2.connect(`http://127.0.0.1:${port}`);
  try {
    const info = JSON.parse((await h2Request(client, '/info')).body);
    assert.strictEqual(info.alpn, 'h2c');
    assert.strictEqual(info.host, `127.0.0.1:${port}`);
    assert.strictEqual(info.url, `http://127.0.0.1:${port}/info`);

    const echo = await h2Request(client, '/echo', {
      method: 'POST', body: 'y'.repeat(100000),
    });
    assert.strictEqual(echo.body.length, 100000);

    const trailers = await h2Request(client, '/trailers');
    assert.strictEqual(trailers.body, 'data');
    assert.strictEqual(trailers.trailers['x-sum'], '4');
    assert.strictEqual(trailers.trailers['content-type'], undefined);

    const informational = await h2Request(client, '/informational');
    assert.deepStrictEqual(informational.info, [103]);

    await assert.rejects(h2Request(client, '/deny'), {
      rstCode: http2.constants.NGHTTP2_REFUSED_STREAM,
    });
    await assert.rejects(h2Request(client, '/error-response'), {
      rstCode: http2.constants.NGHTTP2_INTERNAL_ERROR,
    });
    assert.strictEqual((await h2Request(client, '/throw')).status, 599);
  } finally {
    client.close();
    await server.close();
  }
});

test('TLS with ALPN', async () => {
  const { server, port } = await startServer({ tls: { key, cert } });
  try {
    const h1 = JSON.parse((await h1Request(port, '/info', { tls: true })).body);
    assert.strictEqual(h1.alpn, 'http/1.1');
    assert.strictEqual(h1.encrypted, true);
    assert.strictEqual(h1.url, `https://127.0.0.1:${port}/info`);

    const client = http2.connect(`https://127.0.0.1:${port}`, {
      ca, servername: 'agent1',
    });
    const h2 = JSON.parse((await h2Request(client, '/info')).body);
    assert.strictEqual(h2.alpn, 'h2');
    assert.strictEqual(h2.encrypted, true);
    client.close();
  } finally {
    await server.close();
  }
});

test('two-step listen and multiple listeners', async () => {
  const server = serve(handler);
  await server.ready;
  assert.deepStrictEqual([...server], []);
  const a = await server.listen({ port: 0, hostname: '127.0.0.1' });
  const b = await server.listen({ port: 0, hostname: '127.0.0.1' });
  assert.deepStrictEqual([...server], [a, b]);
  assert.strictEqual(a.address.family, 'IPv4');
  assert.strictEqual((await h1Request(b.address.port, '/middleware')).body,
                     'http/1.1');
  await a.close();
  assert.deepStrictEqual([...server], [b]);
  await server.close();
  await assert.rejects(server.listen({ port: 0 }), { name: 'InvalidStateError' });
});

test('binding failure rejects ready', async () => {
  const blocker = net.createServer().listen(0, '127.0.0.1');
  await new Promise((resolve) => blocker.once('listening', resolve));
  const server = serve(handler, {
    port: blocker.address().port, hostname: '127.0.0.1',
  });
  await assert.rejects(server.ready, { code: 'EADDRINUSE' });
  await assert.rejects(server.closed, { code: 'EADDRINUSE' });
  blocker.close();
});

test('busy refuses new exchanges', async () => {
  const { server, port } = await startServer();
  try {
    server.busy = true;
    assert.strictEqual([...server][0].busy, true);
    assert.strictEqual((await h1Request(port, '/info')).status, 503);
    server.busy = false;
    assert.strictEqual((await h1Request(port, '/info')).status, 200);
  } finally {
    await server.close();
  }
});

test('destroy and signal', async () => {
  const ac = new AbortController();
  const { server } = await startServer({ signal: ac.signal });
  const reason = new Error('shutting down');
  ac.abort(reason);
  await assert.rejects(server.closed, reason);

  const { server: other } = await startServer();
  other.destroy();
  await assert.rejects(other.closed, { name: 'AbortError' });
});

test('await using closes the server', async () => {
  let port;
  {
    await using server = serve(handler, { port: 0, hostname: '127.0.0.1' });
    await server.ready;
    port = [...server][0].address.port;
  }
  await assert.rejects(h1Request(port, '/info'), { code: 'ECONNREFUSED' });
});

test('request signal is aborted when the client disconnects', async () => {
  const aborted = Promise.withResolvers();
  const server = serve({
    fetch(ctx) {
      ctx.request.signal.addEventListener('abort', () => {
        aborted.resolve(ctx.request.signal.reason);
      });
      ctx.completed.catch(() => {});
      return new Promise(() => {});
    },
  }, { port: 0, hostname: '127.0.0.1' });
  await server.ready;
  const socket = net.connect([...server][0].address.port, '127.0.0.1');
  socket.write('GET / HTTP/1.1\r\nHost: x\r\n\r\n');
  setTimeout(() => socket.destroy(), 50);
  const reason = await aborted.promise;
  assert.strictEqual(reason.name, 'TypeError');
  assert.strictEqual(reason.code, 'ERR_HTTP_CONNECTION_RESET');
  server.destroy();
});

test('content limits', async () => {
  const { server, port } = await startServer({
    limits: { maxRequestBodySize: 10 },
  });
  try {
    const declared = await h1Request(port, '/echo', {
      method: 'POST', body: 'x'.repeat(20),
    });
    assert.strictEqual(declared.status, 413);
    const streamed = await new Promise((resolve) => {
      const req = http.request({
        host: '127.0.0.1', port, path: '/echo', method: 'POST',
      }, (res) => {
        res.resume();
        resolve(res.statusCode);
      });
      req.write('x'.repeat(8));
      setTimeout(() => req.end('y'.repeat(8)), 20);
    });
    assert.strictEqual(streamed, 413);
  } finally {
    await server.close();
  }
});

test('no fetch handler answers 501', async () => {
  const server = serve({ connect() {} }, { port: 0, hostname: '127.0.0.1' });
  await server.ready;
  assert.strictEqual((await h1Request([...server][0].address.port, '/')).status,
                     501);
  await server.close();
});
