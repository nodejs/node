// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: incoming stream consumer checks.
// An incoming stream must not be destroyed just because `onstream` is
// not set: on a session whose application supports headers (HTTP/3),
// session-level stream callbacks (`onheaders` et al) are a consumer
// and the stream must be kept and driven by the application layer.
// Refs: https://github.com/nodejs/node/issues/64192
//
// A session with no stream consumers at all still destroys incoming
// streams (and emits a warning), so unconsumed streams cannot
// accumulate and hold flow control credit.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');
const { text } = await import('stream/iter');

const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');

// The consumer warning must never fire in the first block (onheaders is
// a consumer) and must fire in the second (no runnable consumer).
// common.expectWarning is not usable here: importing node:quic emits
// ExperimentalWarning, which it would reject as unexpected.
const kWarning =
  'A new stream was received but no stream consumer callback was provided';
function failOnConsumerWarning(warning) {
  assert.notStrictEqual(warning.message, kWarning);
}

// --- An h3 request completes with only session-level stream callbacks ---
{
  process.on('warning', failOnConsumerWarning);
  const serverDone = Promise.withResolvers();

  // Note: no `onstream` callback anywhere on this session.
  const serverEndpoint = await listen(mustCall((serverSession) => {
    serverSession.onerror = () => {};
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
    onheaders: mustCall(function(headers) {
      assert.strictEqual(headers[':path'], '/test');
      this.sendHeaders({
        ':status': '200',
        'content-type': 'text/plain',
      });
      const w = this.writer;
      w.writeSync('kept without onstream');
      w.endSync();
      serverDone.resolve();
    }),
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
    verifyPeer: 'manual',
  });
  await clientSession.opened;

  const headersReceived = Promise.withResolvers();
  const stream = await clientSession.createBidirectionalStream({
    headers: {
      ':method': 'GET',
      ':path': '/test',
      ':scheme': 'https',
      ':authority': 'localhost',
    },
    onheaders: mustCall((headers) => {
      assert.strictEqual(headers[':status'], 200);
      headersReceived.resolve();
    }),
  });

  await headersReceived.promise;
  const body = await text(stream);
  assert.strictEqual(body, 'kept without onstream');

  await serverDone.promise;
  await clientSession.close();
  await serverEndpoint.close();
  process.off('warning', failOnConsumerWarning);
}

// --- Stream callbacks that cannot run are not a consumer ---
// On a session whose negotiated application does not support headers,
// registered session-level stream callbacks can never fire, so an
// incoming stream with no onstream callback is destroyed with the
// warning. The h3 block above must not trigger that warning.
{
  // Awaiting warned.promise is the assertion: the test times out if the
  // warning never fires.
  const warned = Promise.withResolvers();
  process.on('warning', function onWarning(warning) {
    if (warning.message === kWarning) {
      process.off('warning', onWarning);
      warned.resolve();
    }
  });

  // The onheaders callback is registered but the ALPN is not h3,
  // so it can never run.
  const serverEndpoint = await listen(mustCall((serverSession) => {
    serverSession.onerror = () => {};
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
    alpn: ['test-proto'],
    onheaders: () => {},
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
    alpn: 'test-proto',
    verifyPeer: 'manual',
  });
  await clientSession.opened;

  const stream = await clientSession.createUnidirectionalStream();
  stream.writer.writeSync('x');

  await warned.promise;
  await clientSession.close();
  await serverEndpoint.close();
}
