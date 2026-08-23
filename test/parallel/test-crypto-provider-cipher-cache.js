// Flags: --expose-internals --no-warnings
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL3 } = require('../common/crypto');
if (!hasOpenSSL3)
  common.skip('this test requires OpenSSL 3.x');

const assert = require('assert');
const {
  createCipheriv,
  getCipherInfo,
  getCiphers,
  getFips,
  getHashes,
  setFips,
} = require('crypto');
const { internalBinding } = require('internal/test/binding');
const { Worker } = require('worker_threads');

const algorithm = 'aes-128-cbc-cts';
const hashAlgorithm = 'md5';
const originalFips = getFips();
setFips(0);

if (!getCiphers().includes(algorithm)) {
  common.skip(`${algorithm} is not supported`);
}
assert(getHashes().includes(hashAlgorithm));

const binding = internalBinding('crypto');
const generation = binding.getFipsCryptoGeneration();
setFips(0);
assert.strictEqual(binding.getFipsCryptoGeneration(), generation);

const ciphers = getCiphers();
ciphers.length = 0;
assert(getCiphers().includes(algorithm));

const info = getCipherInfo(algorithm);
assert(info);
assert.deepStrictEqual(getCipherInfo(algorithm.toUpperCase()), info);
assert.deepStrictEqual(getCipherInfo(algorithm), info);
assert.strictEqual(getCipherInfo('node-test-unknown-provider-cipher'), undefined);
assert.strictEqual(getCipherInfo('node-test-unknown-provider-cipher'), undefined);

const key = Buffer.alloc(16);
const iv = Buffer.alloc(16);
const plaintext = Buffer.alloc(32);
const liveCipher = createCipheriv(algorithm, key, iv);

const worker = new Worker(`
  'use strict';
  const {
    createHash,
    createCipheriv,
    getCipherInfo,
    getCiphers,
    getHashes,
  } = require('crypto');
  const { internalBinding } = require('internal/test/binding');
  const { parentPort, workerData } = require('worker_threads');

  const binding = internalBinding('crypto');
  const key = Buffer.from(workerData.key);
  const iv = Buffer.from(workerData.iv);
  const plaintext = Buffer.from(workerData.plaintext);
  const liveCipher = createCipheriv(workerData.algorithm, key, iv);

  getHashes();
  getCiphers();
  getCipherInfo(workerData.algorithm);
  parentPort.postMessage({
    phase: 'warm',
    generation: binding.getFipsCryptoGeneration(),
  });

  parentPort.on('message', (phase) => {
    if (phase === 'fips-on') {
      let errorCode;
      try {
        createCipheriv(workerData.algorithm, key, iv);
      } catch (error) {
        errorCode = error.code;
      }
      const output = Buffer.concat([
        liveCipher.update(plaintext),
        liveCipher.final(),
      ]);
      parentPort.postMessage({
        phase,
        errorCode,
        generation: binding.getFipsCryptoGeneration(),
        hasCipher: getCiphers().includes(workerData.algorithm),
        hasHash: getHashes().includes(workerData.hashAlgorithm),
        hasInfo: getCipherInfo(workerData.algorithm) !== undefined,
        outputLength: output.length,
      });
    } else if (phase === 'fips-off') {
      const cipher = createCipheriv(workerData.algorithm, key, iv);
      const hash = createHash(workerData.hashAlgorithm).digest('hex');
      const output = Buffer.concat([
        cipher.update(plaintext),
        cipher.final(),
      ]);
      parentPort.postMessage({
        phase,
        generation: binding.getFipsCryptoGeneration(),
        hasHash: getHashes().includes(workerData.hashAlgorithm),
        hasCipher: getCiphers().includes(workerData.algorithm),
        hasInfo: getCipherInfo(workerData.algorithm) !== undefined,
        hash,
        outputLength: output.length,
      });
    } else {
      parentPort.close();
    }
  });
`, {
  eval: true,
  workerData: { algorithm, hashAlgorithm, key, iv, plaintext },
});

let enabledGeneration;
worker.on('message', common.mustCall((message) => {
  if (message.phase === 'warm') {
    assert.strictEqual(message.generation, generation);

    setFips(1);
    enabledGeneration = binding.getFipsCryptoGeneration();
    assert.strictEqual(enabledGeneration, generation + 1n);
    assert(!getCiphers().includes(algorithm));
    assert(!getHashes().includes(hashAlgorithm));
    assert.strictEqual(getCipherInfo(algorithm), undefined);
    assert.throws(() => createCipheriv(algorithm, key, iv), {
      code: 'ERR_CRYPTO_UNKNOWN_CIPHER',
    });

    const output = Buffer.concat([
      liveCipher.update(plaintext),
      liveCipher.final(),
    ]);
    assert.strictEqual(output.length, plaintext.length);
    worker.postMessage('fips-on');
  } else if (message.phase === 'fips-on') {
    assert.strictEqual(message.generation, enabledGeneration);
    assert.strictEqual(message.hasCipher, false);
    assert.strictEqual(message.hasHash, false);
    assert.strictEqual(message.hasInfo, false);
    assert.strictEqual(message.errorCode, 'ERR_CRYPTO_UNKNOWN_CIPHER');
    assert.strictEqual(message.outputLength, plaintext.length);

    setFips(0);
    assert.strictEqual(
      binding.getFipsCryptoGeneration(), enabledGeneration + 1n);
    assert(getHashes().includes(hashAlgorithm));
    assert(getCiphers().includes(algorithm));
    assert(getCipherInfo(algorithm));
    worker.postMessage('fips-off');
  } else {
    assert.strictEqual(message.phase, 'fips-off');
    assert.strictEqual(
      message.generation, binding.getFipsCryptoGeneration());
    assert.strictEqual(message.hasCipher, true);
    assert.strictEqual(message.hasHash, true);
    assert.strictEqual(message.hasInfo, true);
    assert.strictEqual(
      message.hash,
      'd41d8cd98f00b204e9800998ecf8427e',
    );
    assert.strictEqual(message.outputLength, plaintext.length);
    worker.postMessage('done');
    setFips(originalFips);
  }
}, 3));
worker.on('error', common.mustNotCall());
worker.on('exit', common.mustCall((code) => assert.strictEqual(code, 0)));
