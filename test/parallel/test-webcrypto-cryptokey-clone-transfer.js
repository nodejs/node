'use strict';

// Tests that CryptoKey instances can be structured-cloned (same-realm
// via `structuredClone`, cross-realm via `MessagePort.postMessage` and
// `Worker.postMessage`) and that the clones:
//   1. preserve all of [[type]], [[extractable]], [[algorithm]],
//      [[usages]] internal slots (as observed through both the public
//      accessors and the custom util.inspect output),
//   2. are usable in cryptographic operations (sign/verify/encrypt/
//      decrypt/exportKey) and produce the same output as the original,
//   3. can themselves be cloned again (round-trip), and
//   4. work for secret, public, and private keys and for both
//      extractable and non-extractable keys.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('node:assert');
const { inspect } = require('node:util');
const { once } = require('node:events');
const { Worker, MessageChannel } = require('node:worker_threads');
const { subtle } = globalThis.crypto;

function assertNoOwnReflection(key) {
  assert.deepStrictEqual(Object.getOwnPropertySymbols(key), []);
  assert.deepStrictEqual(Object.getOwnPropertyNames(key), []);
  assert.deepStrictEqual(Reflect.ownKeys(key), []);
}

function describeKey(key) {
  const algorithm = { ...key.algorithm };
  if (algorithm.hash !== undefined)
    algorithm.hash = { ...algorithm.hash };
  if (algorithm.publicExponent !== undefined)
    algorithm.publicExponent = Array.from(algorithm.publicExponent);
  return {
    type: key.type,
    extractable: key.extractable,
    algorithm,
    usages: [...key.usages].sort(),
  };
}

function assertSameCryptoKey(a, b) {
  assert.notStrictEqual(a, b);
  assert.strictEqual(a.type, b.type);
  assert.strictEqual(a.extractable, b.extractable);
  assert.deepStrictEqual(a.algorithm, b.algorithm);
  assert.deepStrictEqual([...a.usages].sort(), [...b.usages].sort());
  assertNoOwnReflection(a);
  assertNoOwnReflection(b);
  // util.inspect reads native internal slots directly, so a clone's
  // rendered form must match the original's.
  assert.strictEqual(inspect(a, { depth: 4 }), inspect(b, { depth: 4 }));
  // assert.deepStrictEqual on CryptoKey objects goes through the
  // dedicated isCryptoKey branch in comparisons.js; a clone must be
  // deep-equal to its source.
  assert.deepStrictEqual(a, b);
}

async function roundTripViaMessageChannel(key) {
  const { port1, port2 } = new MessageChannel();
  port1.postMessage(key);
  const [received] = await once(port2, 'message');
  port1.close();
  port2.close();
  return received;
}

async function checkHmacKey(original) {
  const data = Buffer.from('some data to sign');

  const cloned = structuredClone(original);
  assertSameCryptoKey(original, cloned);

  const viaPort = await roundTripViaMessageChannel(original);
  assertSameCryptoKey(original, viaPort);

  // Round-trip: clone a clone.
  const clonedAgain = structuredClone(viaPort);
  assertSameCryptoKey(original, clonedAgain);
  const viaPortAgain = await roundTripViaMessageChannel(cloned);
  assertSameCryptoKey(original, viaPortAgain);

  // Signatures produced by every copy must match.
  const sigs = await Promise.all(
    [original, cloned, viaPort, clonedAgain, viaPortAgain].map(
      (k) => subtle.sign('HMAC', k, data),
    ),
  );
  for (let i = 1; i < sigs.length; i++) {
    assert.deepStrictEqual(Buffer.from(sigs[0]), Buffer.from(sigs[i]));
  }

  // Each copy must verify a signature produced by any other copy.
  for (const verifier of [original, cloned, viaPort, clonedAgain]) {
    for (const sig of sigs) {
      assert.strictEqual(
        await subtle.verify('HMAC', verifier, sig, data), true);
    }
  }

  // Exported JWK must match byte-for-byte when extractable.
  if (original.extractable) {
    const jwks = await Promise.all(
      [original, cloned, viaPort, clonedAgain].map(
        (k) => subtle.exportKey('jwk', k),
      ),
    );
    for (let i = 1; i < jwks.length; i++) {
      assert.deepStrictEqual(jwks[0], jwks[i]);
    }
  } else {
    // Non-extractable keys must refuse export on every copy.
    for (const k of [cloned, viaPort, clonedAgain]) {
      await assert.rejects(subtle.exportKey('jwk', k),
                           { name: 'InvalidAccessError' });
    }
  }
}

async function checkAsymmetricKeyPair({ publicKey, privateKey }) {
  const data = Buffer.from('payload');

  for (const original of [publicKey, privateKey]) {
    const cloned = structuredClone(original);
    assertSameCryptoKey(original, cloned);
    const viaPort = await roundTripViaMessageChannel(original);
    assertSameCryptoKey(original, viaPort);
    const clonedAgain = structuredClone(viaPort);
    assertSameCryptoKey(original, clonedAgain);
  }

  // Sign with the original private key, verify with every cloned public key.
  const signature = await subtle.sign(
    { name: 'ECDSA', hash: 'SHA-256' }, privateKey, data);
  const publicClones = [
    publicKey,
    structuredClone(publicKey),
    await roundTripViaMessageChannel(publicKey),
    structuredClone(await roundTripViaMessageChannel(publicKey)),
  ];
  for (const pub of publicClones) {
    assert.strictEqual(
      await subtle.verify({ name: 'ECDSA', hash: 'SHA-256' },
                          pub, signature, data),
      true);
  }

  // Sign with every cloned private key, verify with the original public key.
  const privateClones = [
    structuredClone(privateKey),
    await roundTripViaMessageChannel(privateKey),
    structuredClone(await roundTripViaMessageChannel(privateKey)),
  ];
  for (const priv of privateClones) {
    const sig = await subtle.sign(
      { name: 'ECDSA', hash: 'SHA-256' }, priv, data);
    assert.strictEqual(
      await subtle.verify({ name: 'ECDSA', hash: 'SHA-256' },
                          publicKey, sig, data),
      true);
  }
}

async function checkTransferToWorker(key) {
  // A one-shot worker that receives a key, asserts its properties,
  // signs with it, and echoes the key back together with the signature.
  const worker = new Worker(`
    'use strict';
    const { parentPort } = require('node:worker_threads');
    const { subtle } = globalThis.crypto;
    parentPort.once('message', async ({ key, expected }) => {
      try {
        if (key.type !== expected.type ||
            key.extractable !== expected.extractable ||
            key.algorithm.name !== expected.algorithm.name ||
            key.algorithm.hash?.name !== expected.algorithm.hash?.name) {
          throw new Error('slot mismatch in worker');
        }
        const sig = await subtle.sign('HMAC', key, Buffer.from('wdata'));
        // Echo the key back so the parent can verify round-trip.
        parentPort.postMessage({ key, sig: Buffer.from(sig) });
      } catch (err) {
        parentPort.postMessage({ error: err.message });
      }
    });
  `, { eval: true });

  worker.postMessage({
    key,
    expected: {
      type: key.type,
      extractable: key.extractable,
      algorithm: {
        name: key.algorithm.name,
        hash: key.algorithm.hash ? { name: key.algorithm.hash.name } : undefined,
      },
    },
  });
  const [msg] = await once(worker, 'message');
  await worker.terminate();

  assert.strictEqual(msg.error, undefined, msg.error);
  // The key echoed back from the worker must itself be a fully-formed
  // CryptoKey with all slots preserved.
  assertSameCryptoKey(key, msg.key);
  // The signature produced inside the worker must verify against the
  // parent-side key.
  assert.strictEqual(
    await subtle.verify('HMAC', key, msg.sig, Buffer.from('wdata')),
    true);
}

async function checkRsaPssTransferToWorker({ publicKey, privateKey }) {
  const data = Buffer.from('rsa-pss worker payload');
  const algorithm = { name: 'RSA-PSS', saltLength: 32 };
  const parentSignature = Buffer.from(
    await subtle.sign(algorithm, privateKey, data));

  const worker = new Worker(`
    'use strict';
    const assert = require('node:assert');
    const { parentPort } = require('node:worker_threads');
    const { subtle } = globalThis.crypto;

    ${describeKey}

    parentPort.once('message', async (message) => {
      try {
        const {
          publicKey,
          privateKey,
          expectedPublic,
          expectedPrivate,
          signature,
          data,
        } = message;
        assert.deepStrictEqual(describeKey(publicKey), expectedPublic);
        assert.deepStrictEqual(describeKey(privateKey), expectedPrivate);

        const algorithm = { name: 'RSA-PSS', saltLength: 32 };
        const verified = await subtle.verify(
          algorithm, publicKey, signature, data);
        const workerSignature = Buffer.from(
          await subtle.sign(algorithm, privateKey, data));

        parentPort.postMessage({
          publicKey,
          privateKey,
          verified,
          signature: workerSignature,
        });
      } catch (err) {
        parentPort.postMessage({ error: err.stack || err.message });
      }
    });
  `, { eval: true });

  worker.postMessage({
    publicKey,
    privateKey,
    expectedPublic: describeKey(publicKey),
    expectedPrivate: describeKey(privateKey),
    signature: parentSignature,
    data,
  });
  const [msg] = await once(worker, 'message');
  await worker.terminate();

  assert.strictEqual(msg.error, undefined, msg.error);
  assert.strictEqual(msg.verified, true);
  assertSameCryptoKey(publicKey, msg.publicKey);
  assertSameCryptoKey(privateKey, msg.privateKey);
  assert.strictEqual(
    await subtle.verify(algorithm, publicKey, msg.signature, data),
    true);
}

(async () => {
  // Extractable HMAC (secret)
  const hmacExtractable = await subtle.importKey(
    'raw',
    Buffer.from(
      '000102030405060708090a0b0c0d0e0f' +
      '101112131415161718191a1b1c1d1e1f', 'hex'),
    { name: 'HMAC', hash: 'SHA-256' },
    true,
    ['sign', 'verify']);
  await checkHmacKey(hmacExtractable);
  await checkTransferToWorker(hmacExtractable);

  // Non-extractable HMAC (secret)
  const hmacNonExtractable = await subtle.generateKey(
    { name: 'HMAC', hash: 'SHA-384' },
    false,
    ['sign', 'verify']);
  await checkHmacKey(hmacNonExtractable);
  await checkTransferToWorker(hmacNonExtractable);

  // AES-GCM secret key
  {
    const key = await subtle.generateKey(
      { name: 'AES-GCM', length: 256 }, true, ['encrypt', 'decrypt']);
    const cloned = structuredClone(key);
    assertSameCryptoKey(key, cloned);
    const viaPort = await roundTripViaMessageChannel(key);
    assertSameCryptoKey(key, viaPort);
    const clonedAgain = structuredClone(viaPort);
    assertSameCryptoKey(key, clonedAgain);

    const iv = globalThis.crypto.getRandomValues(new Uint8Array(12));
    const plaintext = Buffer.from('secret payload');
    const ciphertext = await subtle.encrypt(
      { name: 'AES-GCM', iv }, key, plaintext);
    // Decrypt with every clone.
    for (const k of [cloned, viaPort, clonedAgain]) {
      const decrypted = await subtle.decrypt(
        { name: 'AES-GCM', iv }, k, ciphertext);
      assert.deepStrictEqual(Buffer.from(decrypted), plaintext);
    }
  }

  // ECDSA keypair (public extractable, private non-extractable)
  const ecKeypair = await subtle.generateKey(
    { name: 'ECDSA', namedCurve: 'P-256' },
    false,
    ['sign', 'verify']);
  await checkAsymmetricKeyPair(ecKeypair);

  // ECDSA with extractable private key (covers the extractable-private path)
  const ecKeypairExtractable = await subtle.generateKey(
    { name: 'ECDSA', namedCurve: 'P-384' },
    true,
    ['sign', 'verify']);
  await checkAsymmetricKeyPair(ecKeypairExtractable);

  // RSA-PSS keypair through a Worker (covers public/private native key
  // handles and cloning of the publicExponent algorithm member).
  const rsaPssKeypair = await subtle.generateKey(
    {
      name: 'RSA-PSS',
      modulusLength: 2048,
      publicExponent: new Uint8Array([1, 0, 1]),
      hash: 'SHA-256',
    },
    false,
    ['sign', 'verify']);
  await checkRsaPssTransferToWorker(rsaPssKeypair);
})().then(common.mustCall());
