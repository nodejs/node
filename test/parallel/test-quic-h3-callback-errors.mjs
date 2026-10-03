// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: HTTP/3 callback error handling.
// Sync throw in onorigin callback destroys the session
// Session errors reach the QuicSession's onerror, then the Http3Session's
// Sync throw in onheaders callback destroys the stream
// Async rejection in onheaders callback destroys the stream
// Sync throw in ontrailers callback destroys the stream
// Sync throw in onwanttrailers callback destroys the stream

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect, Http3Session } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');
const encoder = new TextEncoder();

async function makeServer(onheadersHandler, extraOpts = {}) {
  const done = Promise.withResolvers();
  const ep = await listen(mustCall(async (ss) => {
    ss.onstream = mustCall((stream) => {
      // The server completes its response before the client's
      // callback throws, so the server stream always resolves.
      stream.closed.then(mustCall());
    });
    await ss.closed;
    done.resolve();
  }), {
    alpn: ['h3'],
    sni: { '*': { keys: [key], certs: [cert] } },
    transportParams: { maxIdleTimeout: 1 },
    onheaders: onheadersHandler,
    ...extraOpts,
  });
  return { ep, done };
}

// Sync throw in onheaders callback destroys the stream.
{
  const { ep, done } = await makeServer(
    mustCall(function(headers) {
      this.sendHeaders({ ':status': '200' });
      this.writer.writeSync(encoder.encode('ok'));
      this.writer.endSync();
    }),
  );

  const c = await connect(ep.address, {
    alpn: 'h3',
    servername: 'localhost',
    verifyPeer: 'manual',
    transportParams: { maxIdleTimeout: 1 },
  });
  await c.opened;

  const s = await c.createBidirectionalStream({
    headers: {
      ':method': 'GET',
      ':path': '/',
      ':scheme': 'https',
      ':authority': 'localhost',
    },
    onheaders: mustCall(function() {
      throw new Error('onheaders sync error');
    }),
  });

  await assert.rejects(s.closed, mustCall((err) => {
    assert.strictEqual(err.message, 'onheaders sync error');
    return true;
  }));
  assert.strictEqual(s.destroyed, true);

  c.close();
  await done.promise;
  ep.close();
}

// Async rejection in onheaders callback destroys the stream.
{
  const { ep, done } = await makeServer(
    mustCall(function(headers) {
      this.sendHeaders({ ':status': '200' });
      this.writer.writeSync(encoder.encode('ok'));
      this.writer.endSync();
    }),
  );

  const c = await connect(ep.address, {
    alpn: 'h3',
    servername: 'localhost',
    verifyPeer: 'manual',
    transportParams: { maxIdleTimeout: 1 },
  });
  await c.opened;

  const s = await c.createBidirectionalStream({
    headers: {
      ':method': 'GET',
      ':path': '/',
      ':scheme': 'https',
      ':authority': 'localhost',
    },
    onheaders: mustCall(async function() {
      throw new Error('onheaders async error');
    }),
  });

  await assert.rejects(s.closed, mustCall((err) => {
    assert.strictEqual(err.message, 'onheaders async error');
    return true;
  }));
  assert.strictEqual(s.destroyed, true);

  c.close();
  await done.promise;
  ep.close();
}

// Sync throw in ontrailers callback destroys the stream.
{
  const { ep, done } = await makeServer(
    mustCall(function(headers) {
      this.sendHeaders({ ':status': '200' });
      this.writer.writeSync(encoder.encode('body'));
      this.writer.endSync();
    }),
    {
      onwanttrailers: mustCall(function() {
        this.sendTrailers({ 'x-trailer': 'value' });
      }),
    },
  );

  const c = await connect(ep.address, {
    alpn: 'h3',
    servername: 'localhost',
    verifyPeer: 'manual',
    transportParams: { maxIdleTimeout: 1 },
  });
  await c.opened;

  const s = await c.createBidirectionalStream({
    headers: {
      ':method': 'GET',
      ':path': '/',
      ':scheme': 'https',
      ':authority': 'localhost',
    },
    onheaders: mustCall(function(headers) {
      assert.strictEqual(headers[':status'], 200);
    }),
    ontrailers: mustCall(function() {
      throw new Error('ontrailers sync error');
    }),
  });

  await assert.rejects(s.closed, mustCall((err) => {
    assert.strictEqual(err.message, 'ontrailers sync error');
    return true;
  }));
  assert.strictEqual(s.destroyed, true);

  c.close();
  await done.promise;
  ep.close();
}

// Sync throw in onorigin callback destroys the session.
{
  const serverEndpoint = await listen(mustCall(async (ss) => {
    await ss.closed;
  }), {
    alpn: ['h3'],
    sni: {
      '*': { keys: [key], certs: [cert] },
      'example.com': { keys: [key], certs: [cert] },
    },
    transportParams: { maxIdleTimeout: 1 },
    onheaders(headers) {
      this.sendHeaders({ ':status': '200' });
      this.writer.endSync();
    },
  });

  const quicSession = await connect(serverEndpoint.address, {
    alpn: 'h3',
    autoWrap: false,
    servername: 'example.com',
    verifyPeer: 'manual',
    transportParams: { maxIdleTimeout: 1 },
    onerror: mustCall(function(error) {
      assert.strictEqual(error.message, 'onorigin error');
    }),
  });
  const clientSession = Http3Session.from(quicSession, {
    onorigin: mustCall(function() {
      throw new Error('onorigin error');
    }),
  });
  await clientSession.opened;

  const stream = await clientSession.createBidirectionalStream({
    headers: {
      ':method': 'GET',
      ':path': '/',
      ':scheme': 'https',
      ':authority': 'example.com',
    },
  });

  // The session is destroyed by the callback error, which
  // destroys the stream with the same error.
  await assert.rejects(stream.closed, mustCall((err) => {
    assert.strictEqual(err.message, 'onorigin error');
    return true;
  }));

  await assert.rejects(clientSession.closed, mustCall(() => true));

  serverEndpoint.close();
}

// Sync throw in onwanttrailers callback destroys the
// server stream. The server stream's closed promise rejects with
// the thrown error.
{
  const serverStreamRejected = Promise.withResolvers();
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (ss) => {
    ss.onstream = mustCall(async (stream) => {
      // The server stream rejects because onwanttrailers threw.
      await assert.rejects(stream.closed, mustCall((err) => {
        assert.strictEqual(err.message, 'onwanttrailers error');
        serverStreamRejected.resolve();
        return true;
      }));
    });
    await ss.closed;
    serverDone.resolve();
  }), {
    alpn: ['h3'],
    sni: { '*': { keys: [key], certs: [cert] } },
    transportParams: { maxIdleTimeout: 1 },
    onheaders: mustCall(function(headers) {
      this.sendHeaders({ ':status': '200' });
      this.writer.writeSync(encoder.encode('body'));
      this.writer.endSync();
    }),
    onwanttrailers: mustCall(function() {
      throw new Error('onwanttrailers error');
    }),
  });

  const clientSession = await connect(serverEndpoint.address, {
    alpn: 'h3',
    servername: 'localhost',
    verifyPeer: 'manual',
    transportParams: { maxIdleTimeout: 1 },
  });
  await clientSession.opened;

  const stream = await clientSession.createBidirectionalStream({
    headers: {
      ':method': 'GET',
      ':path': '/',
      ':scheme': 'https',
      ':authority': 'localhost',
    },
    onheaders: mustCall(function(headers) {
      assert.strictEqual(headers[':status'], 200);
    }),
  });

  // Verify the server stream was destroyed by the throw.
  await serverStreamRejected.promise;

  // The client stream is still open (server error doesn't propagate
  // to client automatically). Closing the client session destroys it.
  clientSession.close();
  await Promise.all([stream.closed, serverDone.promise]);
  await serverEndpoint.close();
}

// A session error reaches the QuicSession's onerror first, then the
// Http3Session's, with the same error. A throw in one does not stop the
// other, and surfaces as an uncaught exception like any onerror throw.
{
  const order = [];
  const serverEndpoint = await listen(mustCall(async (quicSession) => {
    quicSession.onerror = () => {};
    await quicSession.closed.catch(() => {});
  }), {
    alpn: ['h3'],
    sni: { '*': { keys: [key], certs: [cert] } },
  });

  const uncaught = Promise.withResolvers();
  process.once('uncaughtException', (err) => uncaught.resolve(err));

  const quicSession = await connect(serverEndpoint.address, {
    alpn: 'h3',
    autoWrap: false,
    servername: 'localhost',
    verifyPeer: 'manual',
    onerror: mustCall(function(err) {
      order.push(['transport', this, err]);
      throw new Error('transport handler failed');
    }),
  });
  const clientSession = Http3Session.from(quicSession, {});
  clientSession.onerror = mustCall(function(err) {
    order.push(['application', this, err]);
  });
  await clientSession.opened;

  const boom = new Error('boom');
  quicSession.destroy(boom);
  assert.deepStrictEqual(order.map(([who]) => who),
                         ['transport', 'application']);
  assert.strictEqual(order[0][1], quicSession);
  assert.strictEqual(order[1][1], clientSession);
  assert.strictEqual(order[0][2], boom);
  assert.strictEqual(order[1][2], boom);

  const err = await uncaught.promise;
  assert.strictEqual(err.error.message, 'transport handler failed');
  assert.strictEqual(err.suppressed, boom);

  await assert.rejects(clientSession.closed, boom);
  await serverEndpoint.close();
}
