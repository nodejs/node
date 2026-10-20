// Flags: --experimental-dtls --no-warnings

// Test: servername must be a string, because it selects the identity the
// peer certificate is checked against.
//
// A non-string reached the binding's IsString() test, failed it, and became a
// null verify host. SSL_set1_host() was then never called, so chain
// verification still ran but hostname verification was silently skipped --
// a certificate for any name the CA had signed would be accepted.

import { hasCrypto, mustNotCall, skip } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { inspect } from 'node:util';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { connect, listen } = await import('node:dtls');

const HOST = '127.0.0.1';
const key = (name) => fixtures.readKey(name).toString();
const ca = key('ca1-cert.pem');

// agent1's certificate is issued for "agent1" by ca1.
const endpoint = listen(() => {}, {
  cert: key('agent1-cert.pem'),
  key: key('agent1-key.pem'),
  host: HOST,
  port: 0,
});
const port = endpoint.address.port;

// A name the certificate is not for is rejected. This is the check that a
// non-string used to skip.
{
  const session = connect(HOST, port, { ca: [ca], servername: 'wrong.example' });
  await assert.rejects(session.opened, { name: 'Error' });
}

// The right name is accepted, so the check is not simply refusing everything.
{
  const session = connect(HOST, port, { ca: [ca], servername: 'agent1' });
  await session.opened;
  assert.strictEqual(session.authorized, true);
  await session.close();
}

// Anything that is not a string is refused up front rather than quietly
// disabling the check.
for (const servername of [{}, [], 42, true, null, Symbol('x'), () => {}]) {
  assert.throws(() => connect(HOST, port, { ca: [ca], servername }), {
    code: 'ERR_INVALID_ARG_TYPE',
  }, `servername: ${inspect(servername)}`);
}

// An object whose toString() would produce a valid name is still refused;
// coercing it would make the identity depend on user-controlled coercion.
assert.throws(() => connect(HOST, port, {
  ca: [ca],
  servername: { toString: mustNotCall() },
}), { code: 'ERR_INVALID_ARG_TYPE' });

// The empty string keeps its documented meaning: no SNI is sent, and the
// identity falls back to the host.
{
  const session = connect(HOST, port, { ca: [ca], servername: '' });
  // Verifying 127.0.0.1 against a certificate for "agent1" must fail.
  await assert.rejects(session.opened, { name: 'Error' });
}

// Omitting it entirely also falls back to the host.
{
  const session = connect(HOST, port, { ca: [ca], servername: undefined });
  await assert.rejects(session.opened, { name: 'Error' });
}

await endpoint.close();
