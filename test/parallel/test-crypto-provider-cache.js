// Flags: --expose-internals --no-warnings
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL, isBoringSSL } = require('../common/crypto');
if (!hasOpenSSL(3) || isBoringSSL)
  common.skip('this test requires OpenSSL 3 provider support');

const assert = require('node:assert');
const crypto = require('node:crypto');
const { once } = require('node:events');
const {
  isMainThread,
  parentPort,
  Worker,
  workerData,
} = require('node:worker_threads');
const { getMacCache } = require('internal/crypto/util');
const { internalBinding } = require('internal/test/binding');

if (!isMainThread && !workerData?.cryptoCacheTest)
  common.skip('crypto.setFips() is not supported in workers');

const binding = internalBinding('crypto');
const getters = ['getCiphers', 'getHashes', 'getMacs', 'getCurves'];
const cipherAlgorithm = 'camellia-128-cbc-cts';
const cipherKey = Buffer.alloc(16);
const iv = Buffer.alloc(16);
const plaintext = Buffer.alloc(32);
const hashAlgorithm = 'md5';
const emptyHash = 'd41d8cd98f00b204e9800998ecf8427e';
const macAlgorithm = 'poly1305';
const macKey = Buffer.from(
  '85d6be7857556d337f4452fe42d506a8' +
  '0103808afb0db2fd4abff6af4149f51b',
  'hex',
);
const macData = Buffer.from('Cryptographic Forum Research Group');
const expectedMac = 'a8061dc1305136c6c22b8baf0c0127a9';
const curve = 'secp256k1';

function checkLists() {
  const lists = {};
  for (const name of getters) {
    const list = crypto[name]();
    assert(list.every((entry) => typeof entry === 'string'), name);
    assert.deepStrictEqual(list, [...list].sort(), name);
    assert.strictEqual(
      new Set(list.map((entry) => entry.toLowerCase())).size,
      list.length,
      name,
    );
    const disposable = crypto[name]();
    assert.notStrictEqual(disposable, list, name);
    disposable.length = 0;
    disposable.push('not-a-real-algorithm');
    assert.deepStrictEqual(crypto[name](), list, name);
    lists[name] = list;
  }
  return lists;
}

function getAliasId(aliases, name) {
  const normalized = name.toLowerCase();
  for (const [alias, id] of Object.entries(aliases)) {
    if (alias.toLowerCase() === normalized) return id;
  }
  return undefined;
}

function checkMacAliases() {
  // Both spellings must refer to one cached EVP_MAC.
  const aliases = binding.getCachedMacAliases();
  const id = getAliasId(aliases, 'kmac128');
  const hyphenatedId = getAliasId(aliases, 'kmac-128');
  if (id === undefined || hyphenatedId === undefined) {
    common.printSkipMessage('KMAC-128 aliases are not available');
    return;
  }
  assert.strictEqual(id, hyphenatedId);
  const options = { outputLength: 32 };
  const key = Buffer.alloc(32, 0x42);
  const data = Buffer.from('cache alias test');
  assert.deepStrictEqual(
    crypto.createMac('KMAC128', key, options).update(data).final(),
    crypto.createMac('KMAC-128', key, options).update(data).final(),
  );
  const after = binding.getCachedMacAliases();
  assert.strictEqual(getAliasId(after, 'kmac128'), id);
  assert.strictEqual(getAliasId(after, 'kmac-128'), id);
}

function createFixtures(lists) {
  const fixtures = {};
  if (lists.getCiphers.includes(cipherAlgorithm)) {
    const info = crypto.getCipherInfo(cipherAlgorithm);
    assert(info);
    assert.deepStrictEqual(
      crypto.getCipherInfo(cipherAlgorithm.toUpperCase()), info);
    assert.deepStrictEqual(crypto.getCipherInfo(cipherAlgorithm), info);
    fixtures.cipher = crypto.createCipheriv(cipherAlgorithm, cipherKey, iv);
  } else {
    common.printSkipMessage(`${cipherAlgorithm} is not supported`);
  }
  for (let i = 0; i < 2; i++) {
    assert.strictEqual(
      crypto.getCipherInfo('node-test-unknown-provider-cipher'), undefined);
  }
  fixtures.hash = lists.getHashes.includes(hashAlgorithm);
  if (lists.getMacs.includes(macAlgorithm)) {
    fixtures.mac = crypto.createMac(macAlgorithm, macKey).update(macData);
    fixtures.macId = getAliasId(binding.getCachedMacAliases(), macAlgorithm);
    assert.strictEqual(typeof fixtures.macId, 'number');
  } else {
    common.printSkipMessage(`${macAlgorithm} is not supported`);
  }
  checkMacAliases();
  if (lists.getCurves.includes(curve)) {
    fixtures.ecdh = crypto.createECDH(curve);
    fixtures.ecdh.generateKeys();
    fixtures.peer = crypto.createECDH(curve).generateKeys();
    fixtures.ecdh.computeSecret(fixtures.peer);
  }
  return fixtures;
}

function checkEnabled(fixtures, available) {
  // Availability comes from a fresh environment. Exercise cached handles before
  // refreshing the warmed JavaScript lists.
  assert(!available.getCiphers.includes(cipherAlgorithm));
  assert(!available.getHashes.includes(hashAlgorithm));
  assert(!available.getCurves.includes(curve));
  if (fixtures.cipher !== undefined) {
    assert.strictEqual(crypto.getCipherInfo(cipherAlgorithm), undefined);
    assert.throws(
      () => crypto.createCipheriv(cipherAlgorithm, cipherKey, iv),
      { code: 'ERR_CRYPTO_UNKNOWN_CIPHER' },
    );
    const output = Buffer.concat([
      fixtures.cipher.update(plaintext), fixtures.cipher.final(),
    ]);
    assert.strictEqual(output.length, plaintext.length);
  }
  if (fixtures.mac !== undefined) {
    if (!available.getMacs.includes(macAlgorithm)) {
      assert.throws(() => crypto.createMac(macAlgorithm, macKey), {
        code: 'ERR_CRYPTO_INVALID_MAC',
      });
      assert.strictEqual(
        getAliasId(binding.getCachedMacAliases(), macAlgorithm), undefined);
    }
    assert.strictEqual(fixtures.mac.final('hex'), expectedMac);
  }
  if (fixtures.ecdh !== undefined) {
    // Existing keys and keys installed after the transition obey current policy.
    const error = { code: 'ERR_CRYPTO_INVALID_KEYPAIR', name: 'RangeError' };
    assert.throws(() => fixtures.ecdh.computeSecret(fixtures.peer), error);
    const installed = crypto.createECDH(curve);
    installed.setPrivateKey(Buffer.from('cafebabe'.repeat(8), 'hex'));
    assert.throws(() => installed.computeSecret(fixtures.peer), error);
  }
}

function checkDisabled(fixtures) {
  if (fixtures.cipher !== undefined) {
    assert(crypto.getCipherInfo(cipherAlgorithm));
    const cipher = crypto.createCipheriv(cipherAlgorithm, cipherKey, iv);
    const output = Buffer.concat([cipher.update(plaintext), cipher.final()]);
    assert.strictEqual(output.length, plaintext.length);
  }
  if (fixtures.hash) {
    assert.strictEqual(crypto.createHash(hashAlgorithm).digest('hex'), emptyHash);
  }
  if (fixtures.mac !== undefined) {
    assert.strictEqual(
      crypto.createMac(macAlgorithm, macKey).update(macData).final('hex'),
      expectedMac,
    );
    const id = getAliasId(binding.getCachedMacAliases(), macAlgorithm);
    assert.strictEqual(typeof id, 'number');
    assert.notStrictEqual(id, fixtures.macId);
  }
  if (fixtures.ecdh !== undefined) fixtures.ecdh.computeSecret(fixtures.peer);
}

function setFips(value) {
  const before = crypto.getFips();
  const generation = binding.getFipsCryptoGeneration();
  try {
    crypto.setFips(value);
  } catch (err) {
    assert.strictEqual(crypto.getFips(), before);
    assert.strictEqual(binding.getFipsCryptoGeneration(), generation);
    throw err;
  }
  assert.strictEqual(crypto.getFips(), value);
  assert.strictEqual(
    binding.getFipsCryptoGeneration(),
    generation + (before === value ? 0n : 1n),
  );
}

// Cached IDs are written back through JavaScript properties. Setter failures
// must propagate for both a stale ID and an uncached (-1) ID.
function withMacCacheSetter(fixtures, toggle) {
  if (fixtures.mac === undefined) return toggle();
  const cache = getMacCache();
  const name = Object.keys(cache).find(
    (name) => name.toLowerCase() === macAlgorithm);
  assert(name);
  const descriptor = Object.getOwnPropertyDescriptor(cache, name);
  assert.strictEqual(descriptor.value, fixtures.macId);
  const sentinel = new Error('mac cache setter');
  function install(id) {
    Object.defineProperty(cache, name, {
      __proto__: null,
      configurable: true,
      enumerable: descriptor.enumerable,
      get() { return id; },
      set() { throw sentinel; },
    });
  }

  function check() {
    assert.throws(() => crypto.createMac(name, macKey), (err) => err === sentinel);
  }
  try {
    install(-1);
    check();
    install(descriptor.value);
    toggle();
    check();
    install(-1);
    check();
  } finally {
    Object.defineProperty(cache, name, descriptor);
  }
}

if (!isMainThread) {
  function reply(phase, lists) {
    parentPort.postMessage({
      phase,
      lists,
      fips: crypto.getFips(),
      generation: binding.getFipsCryptoGeneration(),
    });
  }
  if (workerData.listsOnly) {
    parentPort.once('message', common.mustCall(() => {
      reply('fresh', checkLists());
    }));
    parentPort.postMessage('ready');
  } else {
    let lists = checkLists();
    const fixtures = createFixtures(lists);
    assert.throws(() => setFips(1), {
      code: 'ERR_WORKER_UNSUPPORTED_OPERATION',
    });
    assert.deepStrictEqual(checkLists(), lists);
    reply('warm', lists);
    parentPort.on('message', common.mustCallAtLeast(({ phase, available }) => {
      if (phase === 'done') {
        parentPort.close();
        return;
      }
      if (phase === 'fips-on') checkEnabled(fixtures, available);
      if (phase === 'fips-off') checkDisabled(fixtures);
      const next = checkLists();
      if (phase === 'unchanged') assert.deepStrictEqual(next, lists);
      lists = next;
      reply(phase, lists);
    }));
  }
} else {
  async function main() {
    const originalFips = crypto.getFips();
    const originalLists = checkLists();
    try {
      setFips(0);
    } catch (err) {
      if (err.code !== 'ERR_CRYPTO_FIPS_FORCED') throw err;
      assert.deepStrictEqual(checkLists(), originalLists);
      common.printSkipMessage('FIPS mode cannot be disabled');
      return;
    }
    let worker;
    let freshWorker;
    try {
      const defaultLists = checkLists();
      const fixtures = createFixtures(defaultLists);
      worker = new Worker(__filename, { workerData: { cryptoCacheTest: true } });
      worker.on('error', common.mustNotCall());
      const exitPromise = once(worker, 'exit');
      function checkReply(message, phase, lists) {
        assert.strictEqual(message.phase, phase);
        assert.strictEqual(message.fips, crypto.getFips());
        assert.strictEqual(
          message.generation, binding.getFipsCryptoGeneration());
        assert.deepStrictEqual(message.lists, lists);
      }

      async function exchange(phase, lists) {
        const response = once(worker, 'message');
        worker.postMessage({ phase, available: lists });
        const [message] = await response;
        checkReply(message, phase, lists);
      }
      const [warm] = await once(worker, 'message');
      checkReply(warm, 'warm', defaultLists);

      setFips(0);
      assert.deepStrictEqual(checkLists(), defaultLists);
      await exchange('unchanged', defaultLists);

      // AIX uses OpenSSL entropy when initializing a worker's V8 isolate.
      // Start it before enabling FIPS properties, which can succeed without a
      // FIPS provider, but defer its first cache lookup until after the toggle.
      freshWorker = new Worker(__filename, {
        workerData: { cryptoCacheTest: true, listsOnly: true },
      });
      freshWorker.on('error', common.mustNotCall());
      const freshExit = once(freshWorker, 'exit');
      const [ready] = await once(freshWorker, 'message');
      assert.strictEqual(ready, 'ready');

      withMacCacheSetter(fixtures, () => setFips(1));
      // A cold environment provides independent expectations for both native
      // and JavaScript caches, including when no FIPS provider is installed.
      const freshResponse = once(freshWorker, 'message');
      freshWorker.postMessage('read');
      const [fresh] = await freshResponse;
      const [freshCode] = await freshExit;
      assert.strictEqual(freshCode, 0);
      const enabledLists = fresh.lists;
      checkReply(fresh, 'fresh', enabledLists);
      checkEnabled(fixtures, enabledLists);
      assert.deepStrictEqual(checkLists(), enabledLists);
      await exchange('fips-on', enabledLists);

      setFips(1);
      assert.deepStrictEqual(checkLists(), enabledLists);
      await exchange('unchanged', enabledLists);

      setFips(0);
      checkDisabled(fixtures);
      assert.deepStrictEqual(checkLists(), defaultLists);
      await exchange('fips-off', defaultLists);

      worker.postMessage({ phase: 'done' });
      const [code] = await exitPromise;
      assert.strictEqual(code, 0);
    } finally {
      if (freshWorker !== undefined && freshWorker.threadId !== -1)
        await freshWorker.terminate();
      if (worker !== undefined && worker.threadId !== -1) await worker.terminate();
      setFips(originalFips);
    }
  }
  main().then(common.mustCall());
}
