// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: HTTP/3 datagrams with SETTINGS_H3_DATAGRAM negotiation.
// Verifies that QUIC datagrams work correctly with H3 sessions, including
// the SETTINGS_H3_DATAGRAM negotiation required by RFC 9297.
// 1. Both sides enableDatagrams: true — datagrams work alongside H3 streams
// 2. Server enableDatagrams: false — client should not be able to send
//    datagrams (peer's SETTINGS_H3_DATAGRAM=0)

import { hasQuic, skip, mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';
const { readKey } = fixtures;

const { ok, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');
const { bytes } = await import('stream/iter');
const { setTimeout: sleep } = await import('timers/promises');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');
const decoder = new TextDecoder();

// Test 1: H3 datagrams with enableDatagrams: true on both sides.
// Datagrams work alongside H3 request/response.
{
  const serverGotDatagram = Promise.withResolvers();
  const clientGotDatagram = Promise.withResolvers();
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (ss) => {
    ss.onstream = mustCall(async (stream) => {
      await stream.closed;
    });
    await serverGotDatagram.promise;
    await sleep(50);
    ss.close();
    serverDone.resolve();
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
    application: { enableDatagrams: true },
    transportParams: { maxDatagramFrameSize: 100 },
    // Server echoes received datagram back to client.
    ondatagram: mustCall(function(data) {
      ok(data instanceof Uint8Array);
      strictEqual(data.byteLength, 3);
      strictEqual(data[0], 10);
      strictEqual(data[1], 20);
      strictEqual(data[2], 30);
      // Echo it back.
      this.sendDatagram(new Uint8Array([42, 43, 44]));
      serverGotDatagram.resolve();
    }),
    onheaders: mustCall(function(headers) {
      this.sendHeaders({ ':status': '200' });
      this.writer.writeSync('ok');
      this.writer.endSync();
    }),
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
    application: { enableDatagrams: true },
    transportParams: { maxDatagramFrameSize: 100 },
    // Client receives datagram from server.
    ondatagram: mustCall(function(data) {
      ok(data instanceof Uint8Array);
      strictEqual(data.byteLength, 3);
      strictEqual(data[0], 42);
      strictEqual(data[1], 43);
      strictEqual(data[2], 44);
      clientGotDatagram.resolve();
    }),
  });
  await clientSession.opened;

  // Datagrams work alongside H3 request/response.
  const stream = await clientSession.createBidirectionalStream({
    headers: {
      ':method': 'GET',
      ':path': '/with-datagram',
      ':scheme': 'https',
      ':authority': 'localhost',
    },
    onheaders: mustCall(function(headers) {
      strictEqual(headers[':status'], '200');
    }),
  });

  // Send datagram from client.
  await clientSession.sendDatagram(new Uint8Array([10, 20, 30]));

  // H3 response body is received.
  const body = await bytes(stream);
  strictEqual(decoder.decode(body), 'ok');
  await stream.closed;

  // Both sides received their datagram.
  await Promise.all([serverGotDatagram.promise, clientGotDatagram.promise]);

  await serverDone.promise;
  clientSession.close();
}

// Test 2: Server has enableDatagrams: false. The peer's H3 SETTINGS
// should indicate SETTINGS_H3_DATAGRAM=0. The client's datagram send
// should return 0 (no datagram sent) because the peer doesn't support
// H3 datagrams.
{
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (ss) => {
    ss.onstream = mustCall(async (stream) => {
      await stream.closed;
      ss.close();
      serverDone.resolve();
    });
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
    // Server explicitly disables H3 datagrams.
    application: { enableDatagrams: false },
    // But transport-level datagrams ARE supported.
    transportParams: { maxDatagramFrameSize: 100 },
    // Server should NOT receive any datagrams.
    ondatagram: mustNotCall(),
    onheaders: mustCall((headers, stream) => {
      stream.sendHeaders({ ':status': '200' });
      stream.writer.writeSync('no-dgram');
      stream.writer.endSync();
    }),
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
    application: { enableDatagrams: true },
    transportParams: { maxDatagramFrameSize: 100 },
  });
  await clientSession.opened;

  const stream = await clientSession.createBidirectionalStream({
    headers: {
      ':method': 'GET',
      ':path': '/no-datagram',
      ':scheme': 'https',
      ':authority': 'localhost',
    },
    onheaders: mustCall((headers) => {
      strictEqual(headers[':status'], '200');
    }),
  });

  // The H3 request triggers SETTINGS exchange. After the server's
  // SETTINGS (with h3_datagram=0) arrive, the client should know
  // the peer doesn't support H3 datagrams.
  const body = await bytes(stream);
  strictEqual(decoder.decode(body), 'no-dgram');

  // Attempt to send a datagram. Since the peer's H3 SETTINGS
  // indicate h3_datagram=0, this should return 0 (not sent).
  const dgId = await clientSession.sendDatagram(new Uint8Array([1, 2, 3]));
  strictEqual(dgId, 0n);

  await Promise.all([stream.closed, serverDone.promise]);
  clientSession.close();
}
