// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: HTTP/3 error code handling.
// H3 application error codes are propagated correctly
// Graceful close uses H3_NO_ERROR - streams complete cleanly

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { strictEqual, rejects } = assert;
const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:http3');
const { createPrivateKey } = await import('node:crypto');
const { bytes } = await import('stream/iter');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');
const decoder = new TextDecoder();

// Server closes with explicit application error code.
// Client's session closed rejects with the error.
{
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (ss) => {
    ss.onstream = mustCall(async (stream) => {
      stream.onheaders = mustCall((headers) => {
        stream.sendHeaders({ ':status': '200' });
        stream.writer.writeSync('ok');
        stream.writer.endSync();
      });
      await stream.closed;
      // Close with an explicit H3 application error code.
      ss.close({ code: 0x101, type: 'application' });
      serverDone.resolve();
    });
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
    verifyPeer: 'manual',
  });
  await clientSession.opened;

  const stream = await clientSession.request({
    ':method': 'GET',
    ':path': '/',
    ':scheme': 'https',
    ':authority': 'localhost',
  }, {
    onheaders: mustCall((headers) => {
      strictEqual(headers[':status'], '200');
    }),
  });

  const body = await bytes(stream);
  strictEqual(decoder.decode(body), 'ok');
  await Promise.all([stream.closed, serverDone.promise]);

  // Client sees the application error code.
  await rejects(clientSession.closed, {
    code: 'ERR_QUIC_APPLICATION_ERROR',
  });

  await serverEndpoint.close();
}

// Graceful close with no explicit error code.
// Both streams complete normally. The close uses H3_NO_ERROR
// which is treated as a clean shutdown (not an error).
{
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (ss) => {
    ss.onstream = mustCall(async (stream) => {
      stream.onheaders = mustCall((headers) => {
        stream.sendHeaders({ ':status': '200' });
        stream.writer.writeSync('ok');
        stream.writer.endSync();
      });
      await stream.closed;
      ss.close();
      serverDone.resolve();
    });
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
    verifyPeer: 'manual',
  });
  await clientSession.opened;

  const stream = await clientSession.request({
    ':method': 'GET',
    ':path': '/',
    ':scheme': 'https',
    ':authority': 'localhost',
  }, {
    onheaders: mustCall((headers) => {
      strictEqual(headers[':status'], '200');
    }),
  });

  const body = await bytes(stream);
  strictEqual(decoder.decode(body), 'ok');
  await stream.closed;

  // Graceful close - session close promise resolves
  // because H3_NO_ERROR is a clean close.
  await serverDone.promise;
  await clientSession.close();
  await serverEndpoint.close();
}
