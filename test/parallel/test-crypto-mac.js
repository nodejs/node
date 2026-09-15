// Flags: --expose-internals

'use strict';

const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
}

const { hasOpenSSL } = require('../common/crypto');

if (!hasOpenSSL(3) || process.features.openssl_is_boringssl) {
  common.skip('OpenSSL 3 EVP_MAC support is required');
}

const assert = require('node:assert');
const crypto = require('node:crypto');
const { encodingsMap } = require('internal/util');
const {
  createHmac,
  createMac,
  createSecretKey,
  getMacs,
} = crypto;
const { finished } = require('node:stream/promises');
const { Transform } = require('node:stream');

assert.strictEqual(crypto.Mac, undefined);

const firstMacs = getMacs();
const secondMacs = getMacs();

assert.notStrictEqual(firstMacs, secondMacs);
assert.deepStrictEqual(firstMacs, [...firstMacs].sort());
assert.strictEqual(firstMacs.length, new Set(firstMacs).size);
assert(firstMacs.every((name) => typeof name === 'string'));
assert(firstMacs.every((name) => name === name.toLowerCase()));
assert(firstMacs.every((name) => !/^\d+(?:\.\d+)+$/.test(name)));

firstMacs.push('not-a-real-mac');
assert(!getMacs().includes('not-a-real-mac'));

const availableMacs = new Set(secondMacs);
if (!availableMacs.has('hmac')) {
  common.printSkipMessage('HMAC is not available from the active providers');
} else {
  const algorithm = 'HMAC';
  const options = { digest: 'sha256' };
  const key = Buffer.from('000102030405060708090a0b0c0d0e0f', 'hex');
  const data = Buffer.from('The quick brown fox jumps over the lazy dog');
  const expected = createHmac('sha256', key).update(data).digest();
  const expectedEmpty = createHmac('sha256', key).digest();
  const expectedEmptyKey = createHmac('sha256', Buffer.alloc(0))
    .update(data)
    .digest();

  assert.deepStrictEqual(
    createMac(algorithm, key, options).update(data.toString()).final(),
    expected,
  );
  assert.deepStrictEqual(
    createMac(algorithm, key, options).final(),
    expectedEmpty,
  );
  assert.deepStrictEqual(
    createMac(algorithm, Buffer.alloc(0), options).update(data).final(),
    expectedEmptyKey,
  );

  const nullPrototypeOptions = Object.assign({ __proto__: null }, options);
  assert.deepStrictEqual(
    createMac(algorithm, key, nullPrototypeOptions).update(data).final(),
    expected,
  );
  const inheritedUnknownOptions = Object.assign(
    { __proto__: { unknown: true } }, options);
  assert.deepStrictEqual(
    createMac(algorithm, key, inheritedUnknownOptions).update(data).final(),
    expected,
  );
  assert.deepStrictEqual(
    createMac(algorithm, key, { ...options, unknown: true })
      .update(data)
      .final(),
    expected,
  );
  const incremental = createMac(algorithm, key, options);
  assert(incremental instanceof Transform);
  assert.strictEqual(incremental.update(data.subarray(0, 10)), incremental);
  assert.strictEqual(incremental.update(Buffer.alloc(0)), incremental);
  incremental.update(data.subarray(10));
  assert.deepStrictEqual(incremental.final(), expected);
  assert.throws(
    () => incremental.update(Buffer.alloc(0)),
    { code: 'ERR_CRYPTO_MAC_FINALIZED' },
  );
  assert.throws(
    () => incremental.final(),
    { code: 'ERR_CRYPTO_MAC_FINALIZED' },
  );

  // Input encodings are handled by update(), while final() accepts an output
  // encoding.
  assert.strictEqual(
    createMac(algorithm, key, options)
      .update(data.toString('hex'), 'hex')
      .final('hex'),
    expected.toString('hex'),
  );
  assert.strictEqual(
    createMac(algorithm, key, options)
      .update(data.toString('base64'), 'base64')
      .final('base64url'),
    expected.toString('base64url'),
  );
  for (const outputEncoding of Object.keys(encodingsMap)) {
    if (outputEncoding === 'buffer') continue;
    assert.strictEqual(
      createMac(algorithm, key, options)
        .update(data)
        .final(outputEncoding),
      expected.toString(outputEncoding),
    );
  }
  assert.deepStrictEqual(
    createMac(algorithm, key, options)
      .update(data, 'not-an-encoding')
      .final(),
    expected,
  );
  const explicitBuffer = createMac(algorithm, key, options)
    .update(data)
    .final('buffer');
  assert(Buffer.isBuffer(explicitBuffer));
  assert.deepStrictEqual(explicitBuffer, expected);

  const encodedFinal = createMac(algorithm, key, options).update(data);
  assert.strictEqual(encodedFinal.final('hex'), expected.toString('hex'));
  assert.throws(
    () => encodedFinal.update(data),
    { code: 'ERR_CRYPTO_MAC_FINALIZED' },
  );
  assert.throws(
    () => encodedFinal.final('hex'),
    { code: 'ERR_CRYPTO_MAC_FINALIZED' },
  );
  // BufferSource keys and data must honor view offsets and lengths.
  const keyStorage = Uint8Array.from([0xff, ...key, 0xff]);
  const keyView = new Uint8Array(keyStorage.buffer, 1, key.length);
  const keyDataView = new DataView(keyStorage.buffer, 1, key.length);
  const dataStorage = Uint8Array.from([0xff, ...data, 0xff]);
  const dataView = new DataView(dataStorage.buffer, 1, data.length);
  assert.deepStrictEqual(
    createMac(algorithm, keyView, options).update(dataView).final(),
    expected,
  );
  assert.deepStrictEqual(
    createMac(algorithm, keyDataView, options).update(dataView).final(),
    expected,
  );

  const arrayBufferKey = key.buffer.slice(
    key.byteOffset,
    key.byteOffset + key.byteLength,
  );
  assert.deepStrictEqual(
    createMac(algorithm, arrayBufferKey, options).update(data).final(),
    expected,
  );
  const secretKey = createSecretKey(key);
  assert.deepStrictEqual(
    createMac(algorithm, secretKey, options).update(data).final(),
    expected,
  );

  (async () => {
    const esmCrypto = await import('node:crypto');
    assert.strictEqual(esmCrypto.createMac, createMac);
    assert.strictEqual(esmCrypto.getMacs, getMacs);
    assert.strictEqual(esmCrypto.Mac, undefined);

    const emptyStream = createMac(algorithm, key, options);
    const emptyChunks = [];
    emptyStream.on(
      'data', common.mustCall((chunk) => emptyChunks.push(chunk), 1));
    const emptyFinished = finished(emptyStream);
    emptyStream.end();
    await emptyFinished;
    assert.deepStrictEqual(Buffer.concat(emptyChunks), expectedEmpty);

    const streamed = createMac(algorithm, key, {
      ...options,
      highWaterMark: 1,
    });
    const chunks = [];
    streamed.on('data', common.mustCall((chunk) => chunks.push(chunk), 1));
    assert.strictEqual(streamed.writableHighWaterMark, 1);
    assert.strictEqual(streamed.readableHighWaterMark, 1);
    const streamedFinished = finished(streamed);
    streamed.write(data.subarray(0, 10));
    streamed.end(data.subarray(10));
    await streamedFinished;
    assert.deepStrictEqual(Buffer.concat(chunks), expected);
    assert.throws(
      () => streamed.update(Buffer.alloc(0)),
      { code: 'ERR_CRYPTO_MAC_FINALIZED' },
    );
    assert.throws(
      () => streamed.final(),
      { code: 'ERR_CRYPTO_MAC_FINALIZED' },
    );

    // Direct finalization followed by stream finalization fails through the
    // stream error path and does not emit a second tag.
    const mixed = createMac(algorithm, key, options);
    mixed.on('data', common.mustNotCall());
    const mixedFinished = finished(mixed);
    assert.deepStrictEqual(mixed.update(data).final(), expected);
    mixed.end();
    await assert.rejects(mixedFinished, {
      code: 'ERR_CRYPTO_MAC_FINALIZED',
    });
  })().then(common.mustCall());
}
