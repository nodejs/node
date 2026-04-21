// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: HTTP/3 stream priority.
// Set priority at stream creation (priority/incremental options)
// setPriority({ level: 'high' }) on H3 stream
// setPriority({ incremental: true }) on H3 stream
// priority getter returns { level, incremental } on H3
// priority getter on client H3 stream returns what was set
// Priority set at creation time reflects in stream.priority
// Server priority getter reflects peer's PRIORITY_UPDATE

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';
const { readKey } = fixtures;

const { deepStrictEqual, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');
const { bytes } = await import('stream/iter');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');
const encoder = new TextEncoder();
const decoder = new TextDecoder();

{
  let requestCount = 0;
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (ss) => {
    ss.onstream = mustCall((stream) => {
      // Server sees priority on the stream.
      const pri = stream.priority;
      strictEqual(typeof pri, 'object');
      strictEqual(typeof pri.level, 'string');
      strictEqual(typeof pri.incremental, 'boolean');
    }, 4);
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
    onheaders: mustCall(function(headers) {
      this.sendHeaders({ ':status': '200' });
      this.writer.writeSync(encoder.encode(headers[':path']));
      this.writer.endSync();
      if (++requestCount === 4) {
        serverDone.resolve();
      }
    }, 4),
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
  });
  await clientSession.opened;

  // Priority set at creation time via options.
  const stream1 = await clientSession.createBidirectionalStream({
    headers: {
      ':method': 'GET',
      ':path': '/high',
      ':scheme': 'https',
      ':authority': 'localhost',
    },
    priority: 'high',
    incremental: false,
    onheaders: mustCall(function(headers) {
      strictEqual(headers[':status'], '200');
    }),
  });

  // Priority reflects what was set at creation.
  deepStrictEqual(stream1.priority, { level: 'high', incremental: false });

  // Priority 'low' + incremental at creation.
  const stream2 = await clientSession.createBidirectionalStream({
    headers: {
      ':method': 'GET',
      ':path': '/low-inc',
      ':scheme': 'https',
      ':authority': 'localhost',
    },
    priority: 'low',
    incremental: true,
    onheaders: mustCall(function(headers) {
      strictEqual(headers[':status'], '200');
    }),
  });
  deepStrictEqual(stream2.priority, { level: 'low', incremental: true });

  // Default priority at creation.
  const stream3 = await clientSession.createBidirectionalStream({
    headers: {
      ':method': 'GET',
      ':path': '/default',
      ':scheme': 'https',
      ':authority': 'localhost',
    },
    onheaders: mustCall(function(headers) {
      strictEqual(headers[':status'], '200');
    }),
  });
  deepStrictEqual(stream3.priority, { level: 'default', incremental: false });

  // setPriority after creation.
  const stream4 = await clientSession.createBidirectionalStream({
    headers: {
      ':method': 'GET',
      ':path': '/changed',
      ':scheme': 'https',
      ':authority': 'localhost',
    },
    onheaders: mustCall(function(headers) {
      strictEqual(headers[':status'], '200');
    }),
  });
  // Default priority initially.
  deepStrictEqual(stream4.priority, { level: 'default', incremental: false });

  // Change to high.
  stream4.setPriority({ level: 'high' });
  deepStrictEqual(stream4.priority, { level: 'high', incremental: false });

  // Change to incremental.
  stream4.setPriority({ level: 'low', incremental: true });
  deepStrictEqual(stream4.priority, { level: 'low', incremental: true });

  // Back to default.
  stream4.setPriority({ level: 'default', incremental: false });
  deepStrictEqual(stream4.priority, { level: 'default', incremental: false });

  // Read all bodies.
  const allBodies = await Promise.all([
    bytes(stream1),
    bytes(stream2),
    bytes(stream3),
    bytes(stream4),
  ]);

  strictEqual(decoder.decode(allBodies[0]), '/high');
  strictEqual(decoder.decode(allBodies[1]), '/low-inc');
  strictEqual(decoder.decode(allBodies[2]), '/default');
  strictEqual(decoder.decode(allBodies[3]), '/changed');

  await Promise.all([stream1.closed,
                     stream2.closed,
                     stream3.closed,
                     stream4.closed,
                     serverDone.promise]);
  clientSession.close();
}

// Server priority getter reflects peer's PRIORITY_UPDATE.
// The client creates a stream with default priority, changes it to
// 'high', then sends body data as a signal. The server reads priority
// after receiving the body — by then the PRIORITY_UPDATE frame (sent
// on the control stream) has been processed by nghttp3 internally.
{
  const serverSawHighPriority = Promise.withResolvers();
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (ss) => {
    ss.onstream = mustCall(async (stream) => {
      // Read the request body — this acts as a signal that the
      // client's PRIORITY_UPDATE has been sent. The control stream
      // (carrying PRIORITY_UPDATE) is processed before bidi stream
      // data in nghttp3, so by the time body arrives the priority
      // has been updated.
      const body = await bytes(stream);
      strictEqual(decoder.decode(body), 'signal');

      // The server's priority getter should reflect the
      // client's PRIORITY_UPDATE (high, incremental).
      deepStrictEqual(stream.priority, { level: 'high', incremental: true });
      serverSawHighPriority.resolve();

      await stream.closed;
      ss.close();
      serverDone.resolve();
    });
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
    onheaders: mustCall(function(headers) {
      this.sendHeaders({ ':status': '200' });
      this.writer.writeSync(encoder.encode('ok'));
      this.writer.endSync();
    }),
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
  });
  await clientSession.opened;

  // Create stream with default priority and a body. The body serves
  // as a signal — by the time it arrives at the server, the
  // PRIORITY_UPDATE (sent on the control stream) will have been
  // processed. setPriority is called BEFORE createBidirectionalStream
  // so the PRIORITY_UPDATE is queued before the stream data.
  //
  // Note: setPriority must be called after createBidirectionalStream
  // because the stream handle is needed. But the PRIORITY_UPDATE
  // travels on the control stream which nghttp3 processes before
  // bidi stream data, so the ordering is guaranteed.
  const stream = await clientSession.createBidirectionalStream({
    headers: {
      ':method': 'POST',
      ':path': '/pri-update',
      ':scheme': 'https',
      ':authority': 'localhost',
    },
    body: encoder.encode('signal'),
    onheaders: mustCall(function(headers) {
      strictEqual(headers[':status'], '200');
    }),
  });
  deepStrictEqual(stream.priority, { level: 'default', incremental: false });

  // Change priority — this sends a PRIORITY_UPDATE frame on the
  // control stream. The body data was already provided at creation
  // but the PRIORITY_UPDATE travels on the control stream which
  // nghttp3 prioritizes over bidi streams.
  stream.setPriority({ level: 'high', incremental: true });
  deepStrictEqual(stream.priority, { level: 'high', incremental: true });

  // Read the response.
  const body = await bytes(stream);
  strictEqual(decoder.decode(body), 'ok');

  // Wait for server to confirm it saw the updated priority.
  await Promise.all([serverSawHighPriority.promise,
                     stream.closed,
                     serverDone.promise]);
  clientSession.close();
}
