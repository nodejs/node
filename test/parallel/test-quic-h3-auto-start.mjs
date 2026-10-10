// Flags: --experimental-quic --no-warnings

// Test: a session matching the ALPN is started automatically, unless
// autoStart is false. Servers know the negotiated protocol before surfacing
// a session, and clients offer exactly one.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const {
  listen, connect, Http3Session, QuicSession,
} = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');
const clientOpts = { servername: 'localhost', verifyPeer: 'manual' };

const isHttp3 = (session) => session instanceof Http3Session;

// Both sides start a session by ALPN.
{
  const seen = [];
  const endpoint = await listen(mustCall((session) => {
    seen.push(session.constructor);
    session.onerror = () => {};
  }, 4), {
    alpn: ['h3', 'h3-29', 'other'],
    sni: { '*': { keys: [key], certs: [cert] } },
  });

  // One protocol: started up front, before the handshake.
  const single = await connect(endpoint.address, { ...clientOpts, alpn: 'h3' });
  assert.ok(isHttp3(single));
  assert.throws(() => QuicSession.start(single.connection),
                { code: 'ERR_INVALID_STATE' });
  await single.opened;
  await single.close();

  // The ALPN is read once, so the session always matches what TLS offered.
  let reads = 0;
  const once = await connect(endpoint.address, {
    ...clientOpts,
    get alpn() { return reads++ === 0 ? 'h3' : 'other'; },
  });
  assert.strictEqual(reads, 1);
  assert.ok(isHttp3(once));
  assert.strictEqual((await once.opened).protocol, 'h3');
  await once.close();

  // Draft ALPNs count as HTTP/3 too.
  const draft = await connect(endpoint.address, { ...clientOpts, alpn: 'h3-29' });
  assert.ok(isHttp3(draft));
  await draft.opened;
  await draft.close();

  // Any other protocol is a raw QuicSession on both sides.
  const other = await connect(endpoint.address, { ...clientOpts, alpn: 'other' });
  assert.ok(other instanceof QuicSession);
  await other.opened;
  await other.close();

  await endpoint.close();
  assert.deepStrictEqual(seen, [Http3Session, Http3Session, Http3Session, QuicSession]);
}

// The application option gives the settings for an automatic Http3Session.
{
  const endpoint = await listen(mustCall((session) => {
    assert.strictEqual(session.settings.maxHeaderPairs, 33n);
  }), {
    alpn: ['h3'],
    application: { maxHeaderPairs: 33 },
    sni: { '*': { keys: [key], certs: [cert] } },
  });
  const client = await connect(endpoint.address, {
    ...clientOpts,
    alpn: 'h3',
    application: { qpackBlockedStreams: 7n },
  });
  assert.strictEqual(client.settings.qpackBlockedStreams, 7n);
  await client.opened;
  await client.close();
  await endpoint.close();

  await assert.rejects(connect('127.0.0.1:1', {
    ...clientOpts,
    alpn: 'h3',
    application: { maxHeaderPairs: 'lots' },
  }), { name: 'TypeError', message: /maxHeaderPairs/ });
}

// Session options configure the session autoStart starts, so without it they
// are rejected rather than ignored.
for (const name of ['onerror', 'onstream', 'ondatagram', 'ondatagramstatus',
                    'application']) {
  await assert.rejects(connect('127.0.0.1:1', {
    ...clientOpts,
    alpn: 'h3',
    autoStart: false,
    [name]: name === 'application' ? {} : () => {},
  }), { code: 'ERR_INVALID_ARG_VALUE', message: new RegExp(`options\\.${name}`) });
}

// Autostart option rejects invalid values
for (const autoStart of [1, 'yes', null]) {
  await assert.rejects(connect('127.0.0.1:1', { ...clientOpts, alpn: 'h3', autoStart }),
                       { code: 'ERR_INVALID_ARG_TYPE', message: /options\.autoStart/ });
}
