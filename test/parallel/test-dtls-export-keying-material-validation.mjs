// Flags: --experimental-dtls --no-warnings

// Test: exportKeyingMaterial() rejects bad arguments instead of aborting.
//
// The length went unchecked through both layers and reached
// std::vector<uint8_t>(length). A negative value became a huge size_t and
// terminated the process:
//
//   what():  cannot create std::vector larger than max_size()
//
// -1, 4294967295 and 1e12 all dumped core. The successful cases are covered
// by test-dtls-srtp.mjs; this is about the arguments being refused cleanly.

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

const endpoint = listen(() => {}, {
  cert, key, host: '127.0.0.1', port: 0,
});

const client = connect('127.0.0.1', endpoint.address.port, {
  servername: 'agent1', ca: [ca],
});
await client.opened;

const label = 'EXPORTER-test';

// Lengths that used to abort the process, plus the rest of the bad range.
for (const length of [-1, 0, 4294967295, 1e12, NaN, Infinity, -Infinity, 1.5]) {
  assert.throws(
    () => client.exportKeyingMaterial(length, label),
    { code: 'ERR_OUT_OF_RANGE' },
    `length ${length} should have been refused`);
}

for (const length of ['60', null, {}, [], true]) {
  assert.throws(
    () => client.exportKeyingMaterial(length, label),
    { code: /^ERR_(INVALID_ARG_TYPE|OUT_OF_RANGE)$/ },
    `length ${typeof length} should have been refused`);
}

// The upper bound is enforced, so a caller cannot request a huge allocation.
assert.throws(
  () => client.exportKeyingMaterial(65537, label),
  { code: 'ERR_OUT_OF_RANGE' });
assert.strictEqual(client.exportKeyingMaterial(65536, label).length, 65536);

// The label and context arguments are checked too.
for (const bad of [undefined, 42, null, {}]) {
  assert.throws(
    () => client.exportKeyingMaterial(32, bad),
    { code: 'ERR_INVALID_ARG_TYPE' },
    `label ${typeof bad} should have been refused`);
}

for (const bad of ['ctx', 42, {}]) {
  assert.throws(
    () => client.exportKeyingMaterial(32, label, bad),
    { code: 'ERR_INVALID_ARG_TYPE' },
    `context ${typeof bad} should have been refused`);
}

// A valid call still works, so none of the above is vacuous.
assert.strictEqual(client.exportKeyingMaterial(60, label).length, 60);
assert.strictEqual(
  client.exportKeyingMaterial(32, label, Buffer.from('ctx')).length, 32);

// A destroyed session reports that, rather than reaching the binding.
await client.close();
assert.throws(
  () => client.exportKeyingMaterial(60, label),
  { code: 'ERR_INVALID_STATE' });

await endpoint.close();
