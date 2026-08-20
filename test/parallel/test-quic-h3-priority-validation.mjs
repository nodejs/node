// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: validation of HTTP/3 request priority options.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listenHttp3: listen, connectHttp3: connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');
const { bytes } = await import('stream/iter');

const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');
const decoder = new TextDecoder();

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (session) => {
  session.onstream = mustCall(async (stream) => {
    stream.onheaders = mustCall(() => {
      stream.sendHeaders({ ':status': '200' });
      stream.writer.writeSync('ok');
      stream.writer.endSync();
    });
    await stream.closed;
    session.close();
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

const headers = {
  ':method': 'GET',
  ':path': '/',
  ':scheme': 'https',
  ':authority': 'localhost',
};

// Validation of request() priority/incremental options.
await assert.rejects(
  clientSession.request({ ...headers }, { priority: 'urgent' }),
  { code: 'ERR_INVALID_ARG_VALUE' },
);
await assert.rejects(
  clientSession.request({ ...headers }, { priority: 42 }),
  { code: 'ERR_INVALID_ARG_VALUE' },
);
await assert.rejects(
  clientSession.request({ ...headers }, { incremental: 'yes' }),
  { code: 'ERR_INVALID_ARG_TYPE' },
);
await assert.rejects(
  clientSession.request({ ...headers }, { incremental: 1 }),
  { code: 'ERR_INVALID_ARG_TYPE' },
);

// The rejected requests left nothing behind: validation ran before the
// QUIC stream was opened, so the next request gets the first stream id.
const stream = await clientSession.request({ ...headers }, {
  priority: 'high',
  incremental: true,
});
assert.strictEqual(stream.id, 0n);
assert.deepStrictEqual(stream.priority,
                       { level: 'high', incremental: true });

// setPriority() arguments are validated too.
assert.throws(
  () => stream.setPriority({ level: 'urgent' }),
  { code: 'ERR_INVALID_ARG_VALUE' },
);
assert.throws(
  () => stream.setPriority({ level: 'low', incremental: 'yes' }),
  { code: 'ERR_INVALID_ARG_TYPE' },
);

const body = await bytes(stream);
assert.strictEqual(decoder.decode(body), 'ok');
await Promise.all([stream.closed, serverDone.promise]);
await clientSession.close();
await serverEndpoint.close();
