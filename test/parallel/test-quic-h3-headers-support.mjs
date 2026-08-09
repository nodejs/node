// Flags: --experimental-quic --no-warnings

// Test: QuicStream/QuicSession expose no HTTP/3 surface.
// The HTTP/3 members (sendHeaders, sendInformationalHeaders, sendTrailers,
// headers, onheaders, oninfo, ontrailers, onwanttrailers) were stripped from
// the public node:quic API; the HTTP/3 layer reaches them via internal
// symbols.
// Regardless of the negotiated ALPN, plain QUIC streams must expose none of
// them: the methods are undefined and the callback names are plain inert
// expando properties with no prototype accessors behind them.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect, QuicStream, QuicSession } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');
const encoder = new TextEncoder();

// No h3 accessors or methods exist on the prototypes at all.
for (const name of ['sendHeaders', 'sendInformationalHeaders', 'sendTrailers',
                    'headers', 'pendingTrailers', 'onheaders', 'oninfo',
                    'ontrailers', 'onwanttrailers']) {
  assert.strictEqual(Object.getOwnPropertyDescriptor(QuicStream.prototype, name),
                     undefined);
}
for (const name of ['ongoaway', 'onorigin']) {
  assert.strictEqual(Object.getOwnPropertyDescriptor(QuicSession.prototype, name),
                     undefined);
}

function assertNoH3Surface(stream) {
  assert.strictEqual(typeof stream.sendHeaders, 'undefined');
  assert.strictEqual(typeof stream.sendInformationalHeaders, 'undefined');
  assert.strictEqual(typeof stream.sendTrailers, 'undefined');
  assert.strictEqual(stream.headers, undefined);
  assert.strictEqual(stream.pendingTrailers, undefined);

  // Assigning the old callback names just creates inert expandos; no
  // prototype setter intercepts them.
  const inert = () => {};
  stream.onheaders = inert;
  assert.strictEqual(stream.onheaders, inert);
  delete stream.onheaders;
}

const serverDone = Promise.withResolvers();
const serverEndpoint = await listen(mustCall(async (serverSession) => {
  assert.strictEqual(typeof serverSession.ongoaway, 'undefined');
  assert.strictEqual(typeof serverSession.onorigin, 'undefined');
  serverSession.onstream = mustCall(async (stream) => {
    assertNoH3Surface(stream);

    stream.writer.endSync();

    serverSession.close();
    serverDone.resolve();
  });
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
  alpn: 'quic-test',
});

const clientSession = await connect(serverEndpoint.address, {
  servername: 'localhost',
  verifyPeer: 'manual',
  alpn: 'quic-test',
});
await clientSession.opened;

assert.strictEqual(typeof clientSession.ongoaway, 'undefined');
assert.strictEqual(typeof clientSession.onorigin, 'undefined');

const stream = await clientSession.createBidirectionalStream({
  body: encoder.encode('ping'),
});

// Client side: no h3 surface either.
assertNoH3Surface(stream);

await serverDone.promise;
await clientSession.close();
await serverEndpoint.close();
