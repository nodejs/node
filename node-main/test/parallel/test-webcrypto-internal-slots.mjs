import * as common from '../common/index.mjs';

if (!common.hasCrypto)
  common.skip('missing crypto');

import * as assert from 'node:assert';
import * as util from 'node:util';

const { subtle } = globalThis.crypto;

const kp = await subtle.generateKey('Ed25519', true, ['sign', 'verify']);
assert.notStrictEqual(kp.publicKey.algorithm, kp.privateKey.algorithm);
assert.notStrictEqual(kp.publicKey.usages, kp.privateKey.usages);
kp.publicKey.algorithm.name = 'ed25519';
assert.strictEqual(kp.publicKey.algorithm.name, 'ed25519');
kp.publicKey.usages.push('foo');
assert.ok(kp.publicKey.usages.includes('foo'));
assert.ok(util.inspect(kp.publicKey).includes("algorithm: { name: 'Ed25519' }"));
assert.ok(util.inspect(kp.publicKey).includes("usages: [ 'verify' ]"));

await subtle.sign('Ed25519', kp.privateKey, Buffer.alloc(32));
