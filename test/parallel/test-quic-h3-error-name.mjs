// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Verify that error names are decoded only within the QUIC transport error
// namespace and the CRYPTO_ERROR range.

import { hasQuic, skip, mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');
const { bytes } = await import('stream/iter');

const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');
const decoder = new TextDecoder();

async function receiveCloseError(closeOptions) {
  const serverDone = Promise.withResolvers();
  const serverEndpoint = await listen(mustCall((serverSession) => {
    serverSession.onstream = mustCall(async (stream) => {
      await stream.closed;
      serverSession.close(closeOptions);
      serverDone.resolve();
    });
  }), {
    alpn: ['h3'],
    sni: { '*': { keys: [key], certs: [cert] } },
    onheaders: mustCall(function() {
      this.sendHeaders({ ':status': '200' });
      this.writer.writeSync('ok');
      this.writer.endSync();
    }),
  });

  const clientSession = await connect(serverEndpoint.address, {
    alpn: 'h3',
    servername: 'localhost',
    verifyPeer: 'manual',
  });
  const closed = clientSession.closed.then(
    mustNotCall('session close must not resolve'),
    mustCall((error) => error),
  );
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

  const body = await bytes(stream);
  assert.strictEqual(decoder.decode(body), 'ok');
  await Promise.all([stream.closed, serverDone.promise]);

  const error = await closed;
  await serverEndpoint.close();
  return error;
}

{
  const error = await receiveCloseError({
    code: 0x10an,
    type: 'application',
  });
  assert.strictEqual(error.code, 'ERR_QUIC_APPLICATION_ERROR');
  assert.strictEqual(error.type, 'application');
  assert.strictEqual(error.errorCode, 0x10an);
  assert.strictEqual(error.errorName, undefined);
  assert.strictEqual(error.message, 'QUIC application error 266');
}

{
  const error = await receiveCloseError({
    code: 0x0fn,
    type: 'transport',
  });
  assert.strictEqual(error.code, 'ERR_QUIC_TRANSPORT_ERROR');
  assert.strictEqual(error.type, 'transport');
  assert.strictEqual(error.errorCode, 0x0fn);
  assert.strictEqual(error.errorName, 'AEAD_LIMIT_REACHED');
  assert.strictEqual(
    error.message,
    'QUIC transport error AEAD_LIMIT_REACHED (15)',
  );
}

{
  const error = await receiveCloseError({
    code: 0x300n,
    type: 'transport',
  });
  assert.strictEqual(error.code, 'ERR_QUIC_TRANSPORT_ERROR');
  assert.strictEqual(error.type, 'transport');
  assert.strictEqual(error.errorCode, 0x300n);
  assert.strictEqual(error.errorName, undefined);
  assert.strictEqual(error.message, 'QUIC transport error 768');
}

{
  const error = await receiveCloseError({
    code: 0x10an,
    type: 'transport',
  });
  assert.strictEqual(error.code, 'ERR_QUIC_TRANSPORT_ERROR');
  assert.strictEqual(error.type, 'transport');
  assert.strictEqual(error.errorCode, 0x10an);
  assert.strictEqual(error.errorName, 'unexpected message');
  assert.strictEqual(
    error.message,
    'QUIC transport error unexpected message (266)',
  );
}
