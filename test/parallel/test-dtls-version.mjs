// Flags: --experimental-dtls --no-warnings

// Test: minVersion and maxVersion, and negotiation between DTLS 1.2 and 1.3.

import { hasCrypto, skip } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { connect, createSecureContext, listen } = await import('node:dtls');

const HOST = '127.0.0.1';
const cert = fixtures.readKey('agent1-cert.pem').toString();
const key = fixtures.readKey('agent1-key.pem').toString();

const V12 = { minVersion: 'DTLSv1.2', maxVersion: 'DTLSv1.2' };
const V13 = { minVersion: 'DTLSv1.3', maxVersion: 'DTLSv1.3' };
const BOTH = { minVersion: 'DTLSv1.2', maxVersion: 'DTLSv1.3' };

async function negotiate(serverVersions, clientVersions) {
  const serverProtocol = Promise.withResolvers();
  const endpoint = listen((session) => {
    session.onhandshake = (protocol) => serverProtocol.resolve(protocol);
    session.onmessage = (data) => session.send(data);
    session.onerror = () => {};
  }, { cert, key, host: HOST, port: 0, ...serverVersions });

  const client = connect(HOST, endpoint.address.port, {
    rejectUnauthorized: false, handshakeTimeout: 2000, ...clientVersions,
  });
  try {
    const { protocol } = await client.opened;
    assert.strictEqual(client.protocol, protocol);
    assert.strictEqual(await serverProtocol.promise, protocol);

    // And the association carries data in both directions.
    const echoed = Promise.withResolvers();
    client.onmessage = (data) => echoed.resolve(data.toString());
    client.send('ping');
    assert.strictEqual(await echoed.promise, 'ping');
    await client.close();
    return protocol;
  } finally {
    client.destroy();
    await endpoint.close();
  }
}

// The newest version both sides allow is chosen.
for (const [server, client, expected] of [
  [BOTH, BOTH, 'DTLSv1.3'],
  [BOTH, V13, 'DTLSv1.3'],
  [BOTH, V12, 'DTLSv1.2'],
  [V13, BOTH, 'DTLSv1.3'],
  [V12, BOTH, 'DTLSv1.2'],
  [V13, V13, 'DTLSv1.3'],
  [V12, V12, 'DTLSv1.2'],
]) {
  assert.strictEqual(await negotiate(server, client), expected,
                     `server ${server.maxVersion} client ${client.maxVersion}`);
}

// No common version is a handshake failure on the client, not a fallback.
await assert.rejects(negotiate(V12, V13), /unsupported protocol/);
await assert.rejects(negotiate(V13, V12), /protocol version/);

// The default is DTLS 1.2 on both sides.
assert.strictEqual(await negotiate({}, BOTH), 'DTLSv1.2');
assert.strictEqual(await negotiate(BOTH, {}), 'DTLSv1.2');

// A secure context carries the range.
{
  const context = createSecureContext({ isServer: true, cert, key, ...V13 });
  const endpoint = listen(() => {}, {
    host: HOST, port: 0, secureContext: context,
  });
  const client = connect(HOST, endpoint.address.port, {
    rejectUnauthorized: false, ...BOTH,
  });
  assert.strictEqual((await client.opened).protocol, 'DTLSv1.3');
  await client.close();
  await endpoint.close();

  // ...and cannot be overridden alongside one.
  assert.throws(() => listen(() => {}, {
    host: HOST, port: 0, secureContext: context, maxVersion: 'DTLSv1.2',
  }), { code: 'ERR_INVALID_ARG_VALUE', message: /secureContext/ });
}

// Validation.
for (const name of ['minVersion', 'maxVersion']) {
  for (const bad of ['TLSv1.3', 'DTLSv1', 'DTLSv1.0', 'dtlsv1.3', 12, null]) {
    assert.throws(
      () => createSecureContext({ [name]: bad }),
      { code: 'ERR_INVALID_ARG_VALUE' },
      `${name}: ${bad}`);
  }
}

assert.throws(
  () => createSecureContext({ minVersion: 'DTLSv1.3', maxVersion: 'DTLSv1.2' }),
  { code: 'ERR_INVALID_ARG_VALUE', message: /must not be newer/ });

// Raising only the floor raises the default ceiling with it.
assert.strictEqual(await negotiate({ minVersion: 'DTLSv1.3' }, BOTH),
                   'DTLSv1.3');
await assert.rejects(negotiate({ minVersion: 'DTLSv1.3' }, V12),
                     /protocol version/);
