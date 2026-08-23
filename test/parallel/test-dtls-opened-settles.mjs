// Flags: --experimental-dtls --no-warnings

// Test: session.opened always settles.
//
// close(), destroy() and a peer-initiated close settled `closed` but never
// `opened`. Tearing a session down before its handshake finished therefore
// left anyone awaiting `opened` waiting forever, with no error and no timeout.

import { hasCrypto, skip } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { connect, listen } = await import('node:dtls');

const cert = fixtures.readKey('agent1-cert.pem');
const key = fixtures.readKey('agent1-key.pem');
const ca = fixtures.readKey('ca1-cert.pem');

// Resolves to the promise's outcome, or 'pending' if it has not settled by
// the time the microtask queue and a timer have both drained. Awaiting
// `opened` directly would hang the test rather than fail it.
function outcome(promise) {
  return Promise.race([
    promise.then(() => 'resolved', (err) => err),
    new Promise((resolve) => setTimeout(() => resolve('pending'), 1000)),
  ]);
}

function newClient() {
  const endpoint = listen(() => {}, {
    cert, key, host: '127.0.0.1', port: 0,
  });
  const client = connect('127.0.0.1', endpoint.address.port, {
    servername: 'agent1', ca: [ca],
  });
  return { endpoint, client };
}

// close() before the handshake completes.
{
  const { endpoint, client } = newClient();
  client.close();

  const result = await outcome(client.opened);
  if (result === 'pending') assert.fail('opened must not hang after close()');
  assert.strictEqual(result.code, 'ERR_INVALID_STATE');
  assert.match(result.message, /before the handshake completed/);

  await endpoint.close();
}

// destroy() with no error before the handshake completes.
{
  const { endpoint, client } = newClient();
  client.destroy();

  const result = await outcome(client.opened);
  if (result === 'pending') assert.fail('opened must not hang after destroy()');
  assert.strictEqual(result.code, 'ERR_INVALID_STATE');

  await endpoint.close();
}

// destroy(error) reports that error, rather than a generic one, so a caller
// awaiting opened learns the same thing as one awaiting closed.
{
  const { endpoint, client } = newClient();
  const boom = new Error('boom');
  client.destroy(boom);

  const result = await outcome(client.opened);
  assert.strictEqual(result, boom);

  await endpoint.close();
}

// A completed handshake still resolves, and a later close must not overwrite
// that with a rejection.
{
  const { endpoint, client } = newClient();
  await client.opened;

  await client.close();
  assert.strictEqual(await outcome(client.opened), 'resolved');

  await endpoint.close();
}

// A handshake that fails on its own reports the real reason; teardown must
// not replace it with the generic teardown error.
{
  const endpoint = listen(() => {}, {
    cert, key, host: '127.0.0.1', port: 0,
  });
  // No CA for this server, so verification fails during the handshake.
  const client = connect('127.0.0.1', endpoint.address.port, {
    servername: 'agent1', rejectUnauthorized: true,
  });

  const result = await outcome(client.opened);
  if (result === 'pending') assert.fail('opened must not hang on failure');
  if (result === 'resolved') assert.fail('a failed handshake must not resolve');
  assert.doesNotMatch(result.message, /before the handshake completed/,
                      'the real handshake failure must survive teardown');

  await endpoint.close();
}
