// Flags: --expose-internals --no-warnings
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL3 } = require('../common/crypto');
if (!hasOpenSSL3 || process.features.openssl_is_boringssl)
  common.skip('this test requires OpenSSL 3 EVP_MAC support');

const assert = require('node:assert');
const { once } = require('node:events');
const {
  createMac,
  getFips,
  getMacs,
  setFips,
} = require('node:crypto');
const { getMacCache } = require('internal/crypto/util');
const { internalBinding } = require('internal/test/binding');
const { Worker } = require('node:worker_threads');

const binding = internalBinding('crypto');
const algorithm = 'poly1305';
const key = Buffer.from(
  '85d6be7857556d337f4452fe42d506a8' +
  '0103808afb0db2fd4abff6af4149f51b',
  'hex',
);
const data = Buffer.from('Cryptographic Forum Research Group');
const expected = 'a8061dc1305136c6c22b8baf0c0127a9';
const originalFips = getFips();

function getAliasId(aliases, name) {
  const normalized = name.toLowerCase();
  for (const [alias, id] of Object.entries(aliases)) {
    if (alias.toLowerCase() === normalized) return id;
  }
  return undefined;
}

try {
  setFips(0);
} catch {
  common.skip('FIPS mode cannot be disabled');
}
if (getFips() !== 0)
  common.skip('FIPS mode cannot be disabled');

const initialMacs = getMacs();
if (!initialMacs.includes(algorithm))
  common.skip(`${algorithm} is not supported`);

let fipsMacs;
let canToggleFips = false;
const generationBeforeFipsProbe = binding.getFipsCryptoGeneration();
try {
  setFips(1);
} catch {
  // FIPS mode is optional, so the non-FIPS cache checks below still run.
  assert.strictEqual(
    binding.getFipsCryptoGeneration(),
    generationBeforeFipsProbe,
  );
}
if (getFips() === 1) {
  fipsMacs = getMacs();
  canToggleFips = true;
}
try {
  setFips(0);
} catch {
  canToggleFips = false;
}

const generation = binding.getFipsCryptoGeneration();
setFips(0);
assert.strictEqual(binding.getFipsCryptoGeneration(), generation);

const expectedMacs = getMacs();
const disposableMacs = getMacs();
assert.notStrictEqual(disposableMacs, expectedMacs);
disposableMacs.length = 0;
disposableMacs.push('not-a-real-mac');
assert.deepStrictEqual(getMacs(), expectedMacs);

const aliases = binding.getCachedMacAliases();
const initialAlgorithmId = getAliasId(aliases, algorithm);
assert.strictEqual(typeof initialAlgorithmId, 'number');

const macCache = getMacCache();
const cacheName = Object.keys(macCache).find(
  (name) => name.toLowerCase() === algorithm,
);
assert(cacheName);
const descriptor = Object.getOwnPropertyDescriptor(macCache, cacheName);
assert(descriptor);
assert.strictEqual(descriptor.value, initialAlgorithmId);
const sentinel = new Error('mac cache setter');
const throwsSentinel = (err) => err === sentinel;

function installThrowingMacCacheEntry(id) {
  Object.defineProperty(macCache, cacheName, {
    __proto__: null,
    configurable: true,
    enumerable: descriptor.enumerable,
    get() { return id; },
    set() { throw sentinel; },
  });
}

installThrowingMacCacheEntry(-1);
assert.throws(() => createMac(cacheName, key), throwsSentinel);
Object.defineProperty(macCache, cacheName, descriptor);

// OpenSSL exposes two spellings for each KMAC implementation. They must map
// to the same cached EVP_MAC rather than consume separate cache entries.
const kmac128Id = getAliasId(aliases, 'kmac128');
const kmac128HyphenatedId = getAliasId(aliases, 'kmac-128');
if (kmac128Id === undefined || kmac128HyphenatedId === undefined) {
  common.printSkipMessage('KMAC-128 aliases are not available');
} else {
  assert.strictEqual(kmac128Id, kmac128HyphenatedId);
  const kmacAlgorithm = 'KMAC128';
  const hyphenatedAlgorithm = 'KMAC-128';
  const kmacOptions = { outputLength: 32 };
  const kmacKey = Buffer.alloc(32, 0x42);
  const kmacData = Buffer.from('cache alias test');
  assert.deepStrictEqual(
    createMac(kmacAlgorithm, kmacKey, kmacOptions)
      .update(kmacData).final(),
    createMac(hyphenatedAlgorithm, kmacKey, kmacOptions)
      .update(kmacData).final(),
  );
  const aliasesAfterUse = binding.getCachedMacAliases();
  assert.strictEqual(getAliasId(aliasesAfterUse, 'kmac128'), kmac128Id);
  assert.strictEqual(
    getAliasId(aliasesAfterUse, 'kmac-128'),
    kmac128Id,
  );
}

if (!canToggleFips || fipsMacs.includes(algorithm)) {
  common.printSkipMessage('FIPS cache invalidation cannot be exercised');
  try {
    setFips(originalFips);
  } catch {
    // The process is about to exit and FIPS support is optional.
  }
} else {
  const liveMac = createMac(algorithm, key).update(data);
  const worker = new Worker(`
    'use strict';
    const {
      createMac,
      getFips,
      getMacs,
    } = require('node:crypto');
    const { internalBinding } = require('internal/test/binding');
    const { parentPort, workerData } = require('node:worker_threads');

    function getAliasId(aliases, name) {
      const normalized = name.toLowerCase();
      for (const [alias, id] of Object.entries(aliases)) {
        if (alias.toLowerCase() === normalized) return id;
      }
      return undefined;
    }

    const binding = internalBinding('crypto');
    const key = Buffer.from(workerData.key);
    const data = Buffer.from(workerData.data);
    const liveMac = createMac(workerData.algorithm, key).update(data);
    getMacs();
    const initialAlgorithmId = getAliasId(
      binding.getCachedMacAliases(),
      workerData.algorithm,
    );
    parentPort.postMessage({
      phase: 'warm',
      algorithmId: initialAlgorithmId,
      generation: binding.getFipsCryptoGeneration(),
    });

    parentPort.on('message', (phase) => {
      if (phase === 'fips-on') {
        let errorCode;
        try {
          createMac(workerData.algorithm, key);
        } catch (error) {
          errorCode = error.code;
        }
        const macs = getMacs();
        parentPort.postMessage({
          phase,
          algorithmId: getAliasId(
            binding.getCachedMacAliases(),
            workerData.algorithm,
          ),
          errorCode,
          fips: getFips(),
          generation: binding.getFipsCryptoGeneration(),
          hasAlgorithm: macs.includes(workerData.algorithm),
          tag: liveMac.final('hex'),
        });
      } else if (phase === 'fips-off') {
        const macs = getMacs();
        parentPort.postMessage({
          phase,
          algorithmId: getAliasId(
            binding.getCachedMacAliases(),
            workerData.algorithm,
          ),
          fips: getFips(),
          generation: binding.getFipsCryptoGeneration(),
          hasAlgorithm: macs.includes(workerData.algorithm),
          tag: createMac(workerData.algorithm, key)
            .update(data).final('hex'),
        });
      } else {
        parentPort.close();
      }
    });
  `, {
    eval: true,
    workerData: { algorithm, data, key },
  });
  worker.on('error', common.mustNotCall());

  (async () => {
    const exitPromise = once(worker, 'exit');
    try {
      const [warm] = await once(worker, 'message');
      assert.strictEqual(warm.phase, 'warm');
      assert.strictEqual(typeof warm.algorithmId, 'number');
      assert.strictEqual(warm.generation, generation);

      installThrowingMacCacheEntry(descriptor.value);
      try {
        setFips(1);
        assert.throws(() => createMac(cacheName, key), throwsSentinel);
        installThrowingMacCacheEntry(-1);
        assert.throws(() => createMac(cacheName, key), throwsSentinel);
      } finally {
        Object.defineProperty(macCache, cacheName, descriptor);
      }
      const enabledGeneration = binding.getFipsCryptoGeneration();
      assert.strictEqual(enabledGeneration, generation + 1n);
      assert.strictEqual(getFips(), 1);
      assert(!getMacs().includes(algorithm));
      assert.strictEqual(
        getAliasId(binding.getCachedMacAliases(), algorithm),
        undefined,
      );
      assert.throws(() => createMac(algorithm, key), {
        code: 'ERR_CRYPTO_INVALID_MAC',
      });
      assert.strictEqual(liveMac.final('hex'), expected);

      let responsePromise = once(worker, 'message');
      worker.postMessage('fips-on');
      const [enabled] = await responsePromise;
      assert.strictEqual(enabled.phase, 'fips-on');
      assert.strictEqual(enabled.algorithmId, undefined);
      assert.strictEqual(enabled.errorCode, 'ERR_CRYPTO_INVALID_MAC');
      assert.strictEqual(enabled.fips, 1);
      assert.strictEqual(enabled.generation, enabledGeneration);
      assert.strictEqual(enabled.hasAlgorithm, false);
      assert.strictEqual(enabled.tag, expected);

      setFips(0);
      const disabledGeneration = binding.getFipsCryptoGeneration();
      assert.strictEqual(disabledGeneration, enabledGeneration + 1n);
      assert.strictEqual(getFips(), 0);
      assert(getMacs().includes(algorithm));
      const restoredAlgorithmId = getAliasId(
        binding.getCachedMacAliases(),
        algorithm,
      );
      assert.strictEqual(typeof restoredAlgorithmId, 'number');
      assert.notStrictEqual(restoredAlgorithmId, initialAlgorithmId);
      assert.strictEqual(
        createMac(algorithm, key).update(data).final('hex'),
        expected,
      );

      responsePromise = once(worker, 'message');
      worker.postMessage('fips-off');
      const [disabled] = await responsePromise;
      assert.strictEqual(disabled.phase, 'fips-off');
      assert.strictEqual(disabled.fips, 0);
      assert.strictEqual(disabled.generation, disabledGeneration);
      assert.strictEqual(disabled.hasAlgorithm, true);
      assert.strictEqual(typeof disabled.algorithmId, 'number');
      assert.notStrictEqual(disabled.algorithmId, warm.algorithmId);
      assert.strictEqual(disabled.tag, expected);

      worker.postMessage('done');
      const [code] = await exitPromise;
      assert.strictEqual(code, 0);
    } finally {
      if (worker.threadId !== -1) await worker.terminate();
      setFips(originalFips);
    }
  })().then(common.mustCall());
}
