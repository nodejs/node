// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: HTTP/3 datagrams (RFC 9297) bound to a request stream.
//
// On HTTP/3 sessions a QUIC DATAGRAM frame carries an HTTP/3 Datagram: a
// Quarter Stream ID varint (the request stream id divided by four) followed
// by the payload. Datagrams are therefore sent and received per-stream via
// stream.sendDatagram() / stream.ondatagram rather than at the session level.
//
// 1. Both sides enableDatagrams: true - a datagram round-trips on a stream.
// 2. Server enableDatagrams: false - the client's stream.sendDatagram() returns
//    0n (the peer's SETTINGS_H3_DATAGRAM=0 disables them).
// 3. Raw session.sendDatagram() is rejected on an HTTP/3 session.

import { hasQuic, skip, mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';
const { readKey } = fixtures;

const { ok, strictEqual, deepStrictEqual, rejects } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');

// Test 1: a datagram round-trips on a request stream.
{
  const serverGotDatagram = Promise.withResolvers();
  const clientGotDatagram = Promise.withResolvers();
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (ss) => {
    ss.onstream = mustCall((stream) => {
      stream.ondatagram = mustCall((data, early) => {
        ok(data instanceof Uint8Array);
        deepStrictEqual([...data], [10, 20, 30]);
        strictEqual(early, false);
        // Echo a different payload back on the same stream.
        const id = stream.sendDatagram(new Uint8Array([42, 43, 44]));
        ok(id > 0n);
        serverGotDatagram.resolve();
      });
    });
    await serverGotDatagram.promise;
    await clientGotDatagram.promise;
    ss.close();
    serverDone.resolve();
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
    application: { enableDatagrams: true },
    transportParams: { maxDatagramFrameSize: 100 },
    // Raw session-level datagrams must never be delivered on an H3 session.
    ondatagram: mustNotCall(),
    onheaders: mustCall(function() {
      // Reply with headers only; keep the stream open.
      this.sendHeaders({ ':status': '200' });
    }),
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
    verifyPeer: 'manual',
    application: { enableDatagrams: true },
    transportParams: { maxDatagramFrameSize: 100 },
    ondatagram: mustNotCall(),
  });
  await clientSession.opened;

  const responseHeaders = Promise.withResolvers();
  const stream = await clientSession.createBidirectionalStream({
    headers: {
      ':method': 'GET',
      ':path': '/with-datagram',
      ':scheme': 'https',
      ':authority': 'localhost',
    },
    onheaders: mustCall(function(headers) {
      strictEqual(headers[':status'], '200');
      responseHeaders.resolve();
    }),
  });
  stream.ondatagram = mustCall((data, early) => {
    ok(data instanceof Uint8Array);
    deepStrictEqual([...data], [42, 43, 44]);
    strictEqual(early, false);
    clientGotDatagram.resolve();
  });

  // Wait for the response headers so the server-side stream exists to
  // associate the datagram with, then send.
  await responseHeaders.promise;
  const id = stream.sendDatagram(new Uint8Array([10, 20, 30]));
  ok(id > 0n);

  await Promise.all([serverGotDatagram.promise, clientGotDatagram.promise]);

  // The client received exactly one datagram (the server's echo); the HTTP/3
  // receive path must count it in the datagrams_received stat.
  strictEqual(clientSession.stats.datagramsReceived, 1n);

  stream.destroy();

  await serverDone.promise;
  await clientSession.close();
  await serverEndpoint.close();
}

// Test 2: with the server's SETTINGS_H3_DATAGRAM=0, the client's
// stream.sendDatagram() returns 0n and the server receives nothing.
{
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (ss) => {
    ss.onstream = mustCall((stream) => {
      // The server must never receive an H3 datagram.
      stream.ondatagram = mustNotCall();
    });
    await serverDone.promise;
    ss.close();
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
    // The server disables H3 datagrams...
    application: { enableDatagrams: false },
    // ...even though transport-level datagrams are supported.
    transportParams: { maxDatagramFrameSize: 100 },
    onheaders: mustCall(function() {
      this.sendHeaders({ ':status': '200' });
    }),
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
    verifyPeer: 'manual',
    application: { enableDatagrams: true },
    transportParams: { maxDatagramFrameSize: 100 },
  });
  await clientSession.opened;

  const responseHeaders = Promise.withResolvers();
  const stream = await clientSession.createBidirectionalStream({
    headers: {
      ':method': 'GET',
      ':path': '/no-datagram',
      ':scheme': 'https',
      ':authority': 'localhost',
    },
    onheaders: mustCall(function(headers) {
      strictEqual(headers[':status'], '200');
      responseHeaders.resolve();
    }),
  });

  // After the response headers arrive the server's SETTINGS (h3_datagram=0)
  // have been processed, so a datagram send is not possible.
  await responseHeaders.promise;
  strictEqual(stream.sendDatagram(new Uint8Array([1, 2, 3])), 0n);

  stream.destroy();
  serverDone.resolve();
  await clientSession.close();
  await serverEndpoint.close();
}

// Test 3: raw session-level datagrams are rejected on an HTTP/3 session.
{
  const serverDone = Promise.withResolvers();
  const serverEndpoint = await listen(mustCall(async (ss) => {
    // Accept the request stream so the response (and SETTINGS) flow.
    ss.onstream = mustCall((stream) => stream);
    await serverDone.promise;
    ss.close();
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
    application: { enableDatagrams: true },
    transportParams: { maxDatagramFrameSize: 100 },
    onheaders: mustCall(function() {
      this.sendHeaders({ ':status': '200' });
    }),
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
    verifyPeer: 'manual',
    application: { enableDatagrams: true },
    transportParams: { maxDatagramFrameSize: 100 },
  });
  await clientSession.opened;

  // Issue a request so the HTTP/3 application is fully established before
  // exercising the (blocked) raw datagram path.
  const responseHeaders = Promise.withResolvers();
  const stream = await clientSession.createBidirectionalStream({
    headers: {
      ':method': 'GET',
      ':path': '/',
      ':scheme': 'https',
      ':authority': 'localhost',
    },
    onheaders: mustCall(() => responseHeaders.resolve()),
  });
  await responseHeaders.promise;

  await rejects(clientSession.sendDatagram(new Uint8Array([1, 2, 3])), {
    code: 'ERR_INVALID_STATE',
  });

  stream.destroy();
  serverDone.resolve();
  await clientSession.close();
  await serverEndpoint.close();
}

// Test 4: the SENDER must also have advertised SETTINGS_H3_DATAGRAM=1.
// Per RFC 9297 Section 2.1.1 a datagram MUST NOT be sent until the setting
// has been both sent and received. Here the client (sender) disabled
// datagrams while the server enabled them, so the client cannot send even
// though the peer accepts them.
{
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (ss) => {
    ss.onstream = mustCall((stream) => {
      stream.ondatagram = mustNotCall();
    });
    await serverDone.promise;
    ss.close();
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
    // The server enables H3 datagrams...
    application: { enableDatagrams: true },
    transportParams: { maxDatagramFrameSize: 100 },
    onheaders: mustCall(function() {
      this.sendHeaders({ ':status': '200' });
    }),
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
    verifyPeer: 'manual',
    // ...but the client (the sender) did not advertise support.
    application: { enableDatagrams: false },
    transportParams: { maxDatagramFrameSize: 100 },
  });
  await clientSession.opened;

  const responseHeaders = Promise.withResolvers();
  const stream = await clientSession.createBidirectionalStream({
    headers: {
      ':method': 'GET',
      ':path': '/no-send',
      ':scheme': 'https',
      ':authority': 'localhost',
    },
    onheaders: mustCall(function(headers) {
      strictEqual(headers[':status'], '200');
      responseHeaders.resolve();
    }),
  });

  await responseHeaders.promise;
  // The peer accepts datagrams, but we never advertised, so we cannot send.
  strictEqual(stream.sendDatagram(new Uint8Array([1, 2, 3])), 0n);

  stream.destroy();
  serverDone.resolve();
  await clientSession.close();
  await serverEndpoint.close();
}

// Test 5: a zero-length HTTP Datagram Payload is valid (RFC 9297, "Note that
// this field can be empty") and must round-trip rather than be dropped.
{
  const serverGotDatagram = Promise.withResolvers();
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (ss) => {
    ss.onstream = mustCall((stream) => {
      stream.ondatagram = mustCall((data) => {
        ok(data instanceof Uint8Array);
        strictEqual(data.byteLength, 0);
        serverGotDatagram.resolve();
      });
    });
    await serverGotDatagram.promise;
    ss.close();
    serverDone.resolve();
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
    application: { enableDatagrams: true },
    transportParams: { maxDatagramFrameSize: 100 },
    onheaders: mustCall(function() {
      this.sendHeaders({ ':status': '200' });
    }),
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
    verifyPeer: 'manual',
    application: { enableDatagrams: true },
    transportParams: { maxDatagramFrameSize: 100 },
  });
  await clientSession.opened;

  const responseHeaders = Promise.withResolvers();
  const stream = await clientSession.createBidirectionalStream({
    headers: {
      ':method': 'GET',
      ':path': '/empty-datagram',
      ':scheme': 'https',
      ':authority': 'localhost',
    },
    onheaders: mustCall(() => responseHeaders.resolve()),
  });

  await responseHeaders.promise;
  // An empty payload still produces a non-empty wire datagram (the Quarter
  // Stream ID), so a real datagram id is returned.
  const id = stream.sendDatagram(new Uint8Array(0));
  ok(id > 0n);

  await serverGotDatagram.promise;
  stream.destroy();
  await serverDone.promise;
  await clientSession.close();
  await serverEndpoint.close();
}

// Test 6: a datagram sent on a still-pending stream gets a real, trackable id,
// is delivered once the stream opens, and reports a terminal status via
// ondatagramstatus like any other datagram.
{
  const serverGotDatagram = Promise.withResolvers();
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (ss) => {
    ss.onstream = mustCall((stream) => {
      stream.ondatagram = (data) => {
        deepStrictEqual([...data], [7, 8, 9]);
        serverGotDatagram.resolve();
      };
    }, 2);
    await serverDone.promise;
    ss.close();
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
    application: { enableDatagrams: true },
    // Only one concurrent client bidi stream, so the second request stays
    // pending until the first closes.
    transportParams: { maxDatagramFrameSize: 100, initialMaxStreamsBidi: 1 },
    onheaders: mustCall(function() {
      this.sendHeaders({ ':status': '200' });
      this.writer.endSync();
    }, 2),
  });

  const datagramStatus = Promise.withResolvers();
  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
    verifyPeer: 'manual',
    application: { enableDatagrams: true },
    transportParams: { maxDatagramFrameSize: 100 },
    ondatagramstatus: (id, status) => datagramStatus.resolve({ id, status }),
  });
  await clientSession.opened;

  // First request holds the single available bidi slot.
  const stream1 = await clientSession.createBidirectionalStream({
    headers: {
      ':method': 'GET', ':path': '/first',
      ':scheme': 'https', ':authority': 'localhost',
    },
    onheaders: mustCall(function(headers) {
      strictEqual(headers[':status'], '200');
    }),
  });

  // Second stream is pending; the datagram is buffered with a real id.
  const secondResponse = Promise.withResolvers();
  const stream2 = await clientSession.createBidirectionalStream({
    headers: {
      ':method': 'GET', ':path': '/second',
      ':scheme': 'https', ':authority': 'localhost',
    },
    onheaders: mustCall(() => secondResponse.resolve()),
  });
  ok(stream2.pending);
  const pendingId = stream2.sendDatagram(new Uint8Array([7, 8, 9]));
  ok(pendingId > 0n);

  // Free the slot; stream2 opens and the buffered datagram flushes.
  stream1.writer.endSync();
  for await (const _ of stream1) { /* drain */ } // eslint-disable-line no-unused-vars
  await stream1.closed;

  await Promise.all([secondResponse.promise, serverGotDatagram.promise]);

  // The buffered datagram is tracked: its id reports exactly one terminal
  // status (acknowledged on loopback, but lost is also valid).
  const { id, status } = await datagramStatus.promise;
  strictEqual(id, pendingId);
  ok(status === 'acknowledged' || status === 'lost');

  for await (const _ of stream2) { /* drain */ } // eslint-disable-line no-unused-vars
  await stream2.closed;
  serverDone.resolve();
  await clientSession.close();
  await serverEndpoint.close();
}
