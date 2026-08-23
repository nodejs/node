// Flags: --experimental-dtls --no-warnings

// Test: Option validation for DTLS API.

import { hasCrypto, skip, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import { inspect } from 'node:util';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { connect, createSecureContext, listen } = await import('node:dtls');

// Test: listen() requires a callback.
assert.throws(() => {
  listen(undefined, { cert: 'x', key: 'y', port: 0 });
}, { code: 'ERR_INVALID_ARG_TYPE' });

// Test: listen() requires cert.
assert.throws(() => {
  listen(mustNotCall(), { key: 'y', port: 0 });
}, { code: 'ERR_MISSING_ARGS' });

// Test: listen() requires key.
assert.throws(() => {
  listen(mustNotCall(), { cert: 'x', port: 0 });
}, { code: 'ERR_MISSING_ARGS' });

// Test: listen() requires port.
assert.throws(() => {
  listen(mustNotCall(), { cert: 'x', key: 'y' });
}, { code: 'ERR_MISSING_ARGS' });

// Test: connect() requires valid host.
assert.throws(() => {
  connect(123, 4433);
}, { code: 'ERR_INVALID_ARG_TYPE' });

// Test: connect() requires valid port.
assert.throws(() => {
  connect('localhost', 'invalid');
}, { code: 'ERR_INVALID_ARG_TYPE' });

// Test: connect() rejects out-of-range port.
assert.throws(() => {
  connect('localhost', 99999);
}, { code: 'ERR_OUT_OF_RANGE' });

// Test: mtu must be an integer within [256, 65535].
assert.throws(() => {
  connect('127.0.0.1', 4433, { mtu: 100 });
}, { code: 'ERR_OUT_OF_RANGE' });

assert.throws(() => {
  connect('127.0.0.1', 4433, { mtu: 70000 });
}, { code: 'ERR_OUT_OF_RANGE' });

// Test: alpn must be a string array or Buffer.
assert.throws(() => {
  connect('127.0.0.1', 4433, { alpn: 123 });
}, { code: 'ERR_INVALID_ARG_TYPE' });

// Options that reach a CHECK() in the binding must be rejected in JavaScript.
// A CHECK failure aborts the process rather than throwing, so an unvalidated
// option is a way for a caller to bring the process down with a typo.
{
  const { createSecureContext, DTLSEndpoint } = await import('node:dtls');
  const fixtures = await import('../common/fixtures.mjs');
  const cert = fixtures.readKey('agent1-cert.pem').toString();
  const key = fixtures.readKey('agent1-key.pem').toString();

  // connect() validated the remote host and port but not the local ones.
  for (const bindPort of [1.5, '5000', -1, 65536, null, {}]) {
    assert.throws(() => connect('127.0.0.1', 5684, { bindPort }), {
      code: /^ERR_(INVALID_ARG_TYPE|OUT_OF_RANGE)$/,
    }, `bindPort: ${inspect(bindPort)}`);
  }

  for (const bindHost of [42, null, {}, []]) {
    assert.throws(() => connect('127.0.0.1', 5684, { bindHost }), {
      code: 'ERR_INVALID_ARG_TYPE',
    }, `bindHost: ${inspect(bindHost)}`);
  }

  for (const sessionIdContext of [1, {}, [], true]) {
    assert.throws(() => createSecureContext({
      cert, key, isServer: true, sessionIdContext,
    }), { code: 'ERR_INVALID_ARG_TYPE' },
                  `sessionIdContext: ${inspect(sessionIdContext)}`);
  }

  // The documented 32-byte limit was enforced only by an opaque OpenSSL
  // failure that did not mention the limit.
  assert.throws(() => createSecureContext({
    cert, key, isServer: true, sessionIdContext: 'x'.repeat(33),
  }), { code: 'ERR_OUT_OF_RANGE' });

  // Exactly at the limit is fine.
  createSecureContext({
    cert, key, isServer: true, sessionIdContext: 'x'.repeat(32),
  });

  // A multi-byte character counts for its bytes, not its length.
  assert.throws(() => createSecureContext({
    cert, key, isServer: true, sessionIdContext: 'é'.repeat(17),
  }), { code: 'ERR_OUT_OF_RANGE' });

  assert.throws(() => new DTLSEndpoint(null), {
    code: 'ERR_INVALID_ARG_TYPE',
  });

  // bind(), listen() and connect() on an endpoint were undocumented plumbing
  // whose every argument reached a CHECK(). They are no longer reachable.
  const endpoint = new DTLSEndpoint({});
  assert.strictEqual(endpoint.bind, undefined);
  assert.strictEqual(endpoint.listen, undefined);
  assert.strictEqual(endpoint.connect, undefined);
}

// isServer and rejectUnauthorized are booleans, and are checked rather than
// coerced. Both decide something security-relevant, and both read a value
// that was not a boolean as the opposite of what it looked like:
//
//   createSecureContext({ isServer: 'yes' })  -> a client context
//   connect(..., { rejectUnauthorized: 0 })   -> verification on
//
// Neither failed open, so nothing was unsafe. Both were silent.
{
  for (const value of ['yes', 1, 0, '', null, {}]) {
    assert.throws(() => createSecureContext({ isServer: value }), {
      code: 'ERR_INVALID_ARG_TYPE',
    }, `isServer: ${inspect(value)}`);
  }

  // Booleans still work, and still select the side they name.
  assert.strictEqual(createSecureContext({ isServer: true }).isServer, true);
  assert.strictEqual(createSecureContext({ isServer: false }).isServer, false);
  // Omitted means a client, as documented.
  assert.strictEqual(createSecureContext({}).isServer, false);

  const fixtures = await import('../common/fixtures.mjs');
  const serverCert = fixtures.readKey('agent1-cert.pem').toString();
  const serverKey = fixtures.readKey('agent1-key.pem').toString();

  for (const value of [0, 1, '', 'no', null]) {
    assert.throws(
      () => connect('127.0.0.1', 4433, { rejectUnauthorized: value }),
      { code: 'ERR_INVALID_ARG_TYPE' },
      `connect rejectUnauthorized: ${inspect(value)}`);

    assert.throws(() => listen(() => {}, {
      cert: serverCert,
      key: serverKey,
      host: '127.0.0.1',
      port: 0,
      rejectUnauthorized: value,
    }), { code: 'ERR_INVALID_ARG_TYPE' },
                  `listen rejectUnauthorized: ${inspect(value)}`);
  }
}
