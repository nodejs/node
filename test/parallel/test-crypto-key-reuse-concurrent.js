'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  createPrivateKey,
  createPublicKey,
  diffieHellman,
  generateKeyPairSync,
  KeyObject,
  sign,
  verify,
} = require('crypto');
const { once } = require('events');
const { promisify } = require('util');
const { Worker, parentPort, workerData } = require('worker_threads');
const fixtures = require('../common/fixtures');
const { subtle } = globalThis.crypto;
const signAsync = promisify(sign);
const verifyAsync = promisify(verify);
const ecdsa = { name: 'ECDSA', hash: 'SHA-256' };
const pkcs8 = { type: 'pkcs8', format: 'der' };
const spki = { type: 'spki', format: 'der' };
const iterations = 16;

async function exercise({ keys, peer, expected }) {
  // Check generated signatures and exports with independent imports that do
  // not warm the shared key's provider caches before its first operation.
  const referencePrivate = createPrivateKey({
    key: expected.originalPrivate, ...pkcs8,
  });
  const referencePublic = createPublicKey({ key: expected.public, ...spki });

  for (let i = 0; i < iterations; i++) {
    const data = Buffer.from(`shared key operation ${i}`);
    const referenceSignature = Buffer.from(expected.signatures[i]);
    const pending = [];
    for (let j = 0; j < 4; j++) {
      pending.push(
        verifyAsync('sha256', data, {
          key: keys.publicKey,
          dsaEncoding: 'ieee-p1363',
        }, referenceSignature).then((valid) => {
          assert.strictEqual(valid, true);
        }),
        signAsync('sha256', data, keys.privateKey).then((signature) => {
          assert.strictEqual(
            verify('sha256', data, referencePublic, signature), true);
        }),
      );
      if (keys.ecdsaPrivate === undefined)
        continue;
      pending.push(
        subtle.verify(ecdsa, keys.ecdsaPublic, referenceSignature, data)
          .then((valid) => {
            assert.strictEqual(valid, true);
          }),
        subtle.sign(ecdsa, keys.ecdsaPrivate, data).then((signature) => {
          assert.strictEqual(verify('sha256', data, {
            key: referencePublic,
            dsaEncoding: 'ieee-p1363',
          }, Buffer.from(signature)), true);
        }),
        subtle.deriveBits({ name: 'ECDH', public: peer }, keys.ecdhPrivate, 256)
          .then((bits) => {
            assert.deepStrictEqual(
              Buffer.from(bits), Buffer.from(expected.bits));
          }),
      );
    }

    // Export, compare, and query metadata while jobs use this same native key
    // in the threadpool and other Workers. Fresh wrappers also exercise the
    // native metadata getters on every iteration rather than their JS caches.
    const privateKey = keys.ecdsaPrivate === undefined ?
      structuredClone(keys.privateKey) : KeyObject.from(keys.ecdsaPrivate);
    const publicKey = keys.ecdsaPublic === undefined ?
      structuredClone(keys.publicKey) : KeyObject.from(keys.ecdsaPublic);
    assert.strictEqual(privateKey.equals(referencePrivate), true);
    assert.strictEqual(publicKey.equals(referencePublic), true);
    assert.strictEqual(privateKey.equals(keys.privateKey), true);
    assert.strictEqual(publicKey.equals(keys.publicKey), true);
    assert.deepStrictEqual(privateKey.asymmetricKeyDetails, {
      namedCurve: 'prime256v1',
    });
    assert.deepStrictEqual(publicKey.asymmetricKeyDetails, {
      namedCurve: 'prime256v1',
    });
    assert.deepStrictEqual(
      privateKey.export({ format: 'jwk' }), expected.privateJwk);
    assert.deepStrictEqual(
      publicKey.export({ format: 'jwk' }), expected.publicJwk);
    assert.deepStrictEqual(
      privateKey.export({ format: 'raw-private' }),
      Buffer.from(expected.rawPrivate));
    assert.deepStrictEqual(
      publicKey.export({ format: 'raw-public' }), Buffer.from(expected.rawPublic));
    assert.deepStrictEqual(publicKey.export({
      format: 'raw-public', type: 'compressed',
    }), Buffer.from(expected.compressedPublic));
    assert.deepStrictEqual(
      publicKey.export(spki), Buffer.from(expected.public));

    if (keys.ecdsaPrivate === undefined) {
      assert.deepStrictEqual(diffieHellman({
        privateKey, publicKey: peer,
      }), Buffer.from(expected.bits));
      await Promise.all(pending);
      continue;
    }

    for (const key of [keys.ecdsaPrivate, keys.ecdhPrivate]) {
      pending.push(subtle.exportKey('pkcs8', key).then((encoded) => {
        assert.deepStrictEqual(
          Buffer.from(encoded), Buffer.from(expected.private));
      }));
    }
    pending.push(
      subtle.exportKey('raw', keys.ecdsaPublic).then((encoded) => {
        assert.deepStrictEqual(
          Buffer.from(encoded), Buffer.from(expected.rawPublic));
      }),
      subtle.exportKey('spki', keys.ecdsaPublic).then((encoded) => {
        assert.deepStrictEqual(
          Buffer.from(encoded), Buffer.from(expected.public));
      }),
      subtle.exportKey('jwk', keys.ecdsaPrivate).then((jwk) => {
        assert.deepStrictEqual(jwk, {
          ...expected.privateJwk, key_ops: ['sign'], ext: true,
        });
      }),
    );
    await Promise.all(pending);
  }
}

if (workerData?.sharedKeyTest) {
  parentPort.postMessage('ready');
  Atomics.wait(workerData.barrier, 0, 0);
  exercise(workerData).then(common.mustCall());
} else {
  function der(tag, ...parts) {
    const body = Buffer.concat(parts);
    assert(body.length < 128);
    return Buffer.concat([Buffer.from([tag, body.length]), body]);
  }

  async function run(input, peer, expected, webcrypto = true) {
    // The KeyObject-only case performs no operations before the barrier,
    // exercising concurrent first use of the shared key. CryptoKey conversion
    // validates the key before its first concurrent sign/derive/export calls.
    const privateKey = createPrivateKey({ key: input, ...pkcs8 });
    const publicKey = createPublicKey(privateKey);
    const keys = { privateKey, publicKey };
    if (webcrypto) {
      keys.ecdsaPrivate = privateKey.toCryptoKey({
        name: 'ECDSA', namedCurve: 'P-256',
      }, true, ['sign']);
      keys.ecdsaPublic = publicKey.toCryptoKey({
        name: 'ECDSA', namedCurve: 'P-256',
      }, true, ['verify']);
      keys.ecdhPrivate = privateKey.toCryptoKey({
        name: 'ECDH', namedCurve: 'P-256',
      }, true, ['deriveBits']);
    }
    const barrier = new Int32Array(new SharedArrayBuffer(4));
    const data = { sharedKeyTest: true, keys, peer, expected, barrier };
    const ready = [];
    const exited = [];
    for (let i = 0; i < 3; i++) {
      const worker = new Worker(__filename, { workerData: data });
      ready.push(once(worker, 'message').then(([message]) => {
        assert.strictEqual(message, 'ready');
      }));
      exited.push(once(worker, 'exit').then(common.mustCall(([code]) => {
        assert.strictEqual(code, 0);
      })));
    }
    await Promise.all(ready);
    Atomics.store(barrier, 0, 1);
    Atomics.notify(barrier, 0);
    await Promise.all([exercise(data), ...exited]);

    // Ordinary PKCS8 encoding temporarily changes EC encoding flags in some
    // OpenSSL versions, so compare only after all shared-key users finish.
    // WebCrypto export must add the public point on a copy and leave the
    // original key's encoding flags unchanged.
    assert.deepStrictEqual(
      privateKey.export(pkcs8), Buffer.from(expected.originalPrivate));
  }

  (async () => {
    const reference = createPrivateKey(fixtures.readKey('ec_p256_private.pem'));
    const publicKey = createPublicKey(reference);
    const { publicKey: peer } = generateKeyPairSync('ec', {
      namedCurve: 'prime256v1',
    });
    const privateJwk = reference.export({ format: 'jwk' });
    const expected = {
      private: reference.export(pkcs8),
      public: publicKey.export(spki),
      privateJwk,
      publicJwk: publicKey.export({ format: 'jwk' }),
      rawPrivate: reference.export({ format: 'raw-private' }),
      rawPublic: publicKey.export({ format: 'raw-public' }),
      compressedPublic: publicKey.export({
        format: 'raw-public', type: 'compressed',
      }),
      bits: diffieHellman({ privateKey: reference, publicKey: peer }),
      signatures: Array.from({ length: iterations }, (_, i) => {
        return sign('sha256', Buffer.from(`shared key operation ${i}`), {
          key: reference,
          dsaEncoding: 'ieee-p1363',
        });
      }),
    };
    const cryptoPeer = peer.toCryptoKey({
      name: 'ECDH', namedCurve: 'P-256',
    }, true, []);
    await run(expected.private, peer, {
      ...expected, originalPrivate: expected.private,
    }, false);
    await run(expected.private, cryptoPeer, {
      ...expected, originalPrivate: expected.private,
    });

    const algorithmIdentifier = der(0x30, Buffer.from(
      '06072a8648ce3d020106082a8648ce3d030107', 'hex'));
    const ecPrivateKey = der(
      0x30, Buffer.from('020101', 'hex'),
      der(0x04, Buffer.from(privateJwk.d, 'base64url')));
    const privateOnly = der(
      0x30, Buffer.from('020100', 'hex'), algorithmIdentifier,
      der(0x04, ecPrivateKey));
    const originalPrivate = createPrivateKey({ key: privateOnly, ...pkcs8 })
      .export(pkcs8);
    await run(privateOnly, cryptoPeer, { ...expected, originalPrivate });
  })().then(common.mustCall());
}
