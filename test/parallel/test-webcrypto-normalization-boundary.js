'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { subtle } = globalThis.crypto;

(async () => {
  const data = new Uint8Array(16);
  const key = await subtle.importKey('raw', data, 'AES-CBC', false, ['encrypt']);
  class Parameters {
    #iv = new Uint8Array(16);
    get name() { return 'AES-CBC'; }
    get iv() { return this.#iv; }
  }
  await subtle.encrypt(new Parameters(), key, data);

  const reads = [];
  const algorithm = new Proxy({ name: 'AES-CBC', iv: data }, {
    get: common.mustCall((target, member, receiver) => {
      assert.strictEqual(receiver, algorithm);
      reads.push(member);
      return Reflect.get(target, member, receiver);
    }, 2),
  });
  await subtle.encrypt(algorithm, key, data);
  assert.deepStrictEqual(reads, ['name', 'iv']);

  const decryptKey = await subtle.importKey('raw', data, 'AES-CBC', false, ['decrypt']);
  await assert.rejects(subtle.encrypt({ name: 'AES-CBC', iv: new Uint8Array(8) },
                                      decryptKey, data), { name: 'InvalidAccessError' });
  await assert.rejects(subtle.generateKey({ name: 'AES-CBC', length: 100 }, false, ['sign']),
                       { name: 'SyntaxError' });
  await assert.rejects(subtle.generateKey({ name: 'HMAC', hash: 'SHA-256', length: 0 }, false, ['encrypt']),
                       { name: 'SyntaxError' });
  await assert.rejects(subtle.generateKey({ name: 'AES-CBC', length: 64 }, false, []),
                       { name: 'OperationError' });
  await assert.rejects(subtle.generateKey({ name: 'HMAC', hash: 'SHA-256', length: 0 }, false, []),
                       { name: 'OperationError' });
  if (SubtleCrypto.supports('digest', 'SHA3-256')) {
    const hmac = { name: 'HMAC', hash: 'SHA3-256' };
    await assert.rejects(subtle.generateKey(hmac, false, ['encrypt']), { name: 'SyntaxError' });
    await assert.rejects(subtle.generateKey(hmac, false, []), { name: 'NotSupportedError' });
  }
  await assert.rejects(subtle.generateKey({ name: 'ECDSA', namedCurve: 'P-999' }, false, ['encrypt']),
                       { name: 'SyntaxError' });
  await assert.rejects(subtle.generateKey({ name: 'ECDSA', namedCurve: 'P-999' }, false, []),
                       { name: 'NotSupportedError' });
  await assert.rejects(subtle.generateKey({
    name: 'RSA-PSS', modulusLength: 0,
    publicExponent: new Uint8Array([1, 0, 1]), hash: 'SHA-256',
  }, false, []), { name: 'OperationError' });
  await assert.rejects(subtle.encrypt({ name: 'AES-CTR', counter: new Uint8Array(8), length: NaN },
                                      key, data), TypeError);
  assert.strictEqual(SubtleCrypto.supports('encrypt', { name: 'AES-CBC', iv: new Uint8Array(8) }), false);

  const hkdf = { name: 'HKDF', hash: 'SHA-256', salt: data, info: data };
  const base = await subtle.importKey('raw', data, 'HKDF', false, ['deriveKey']);
  const noDeriveKey = await subtle.importKey('raw', data, 'HKDF', false, ['deriveBits']);
  const hmac = { name: 'HMAC', hash: 'SHA-256', length: 0 };
  await assert.rejects(subtle.deriveKey(hkdf, base, hmac, false, ['sign']), TypeError);
  await assert.rejects(subtle.deriveKey(hkdf, noDeriveKey, hmac, false, ['sign']),
                       { name: 'InvalidAccessError' });
  await assert.rejects(subtle.deriveKey(hkdf, noDeriveKey, { name: 'AES-GCM', length: 100 }, false, ['encrypt']),
                       { name: 'InvalidAccessError' });

  if (SubtleCrypto.supports('importKey', 'Argon2id')) {
    const base = await subtle.importKey('raw-secret', data, 'Argon2id', false, ['deriveBits']);
    const parameters = { name: 'Argon2id', nonce: data, passes: 1, memory: 8, parallelism: 1 };
    const expected = await subtle.deriveBits(parameters, base, 256);
    let conversions = 0;
    const parallelism = { valueOf() { conversions++; return 1.5; } };
    assert.deepStrictEqual(await subtle.deriveBits({ ...parameters, parallelism }, base, 256), expected);
    assert.strictEqual(conversions, 1);
    assert.strictEqual(SubtleCrypto.supports('deriveBits', { ...parameters, parallelism: 1.5 }, 256), true);
  }
})().then(common.mustCall());
