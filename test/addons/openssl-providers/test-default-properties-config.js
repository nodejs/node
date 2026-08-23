'use strict';

const common = require('../../common');
const fixtures = require('../../common/fixtures');
const providers = require('./providers.cjs');

const assert = require('node:assert');
const { fork } = require('node:child_process');
const {
  createHash,
  getHashes,
  hash: oneShotHash,
  setFips,
} = require('node:crypto');
const { Worker } = require('node:worker_threads');
const option = `--openssl-config=${fixtures.path(
  'openssl3-conf',
  'default_properties.cnf',
)}`;

if (!process.execArgv.includes(option)) {
  const cp = fork(__filename, { execArgv: [option] });
  cp.on('exit', common.mustCall((code, signal) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  }));
  return;
}

assert(providers.getCurrentProviders().includes('default'));
assert(providers.getCurrentProviders().includes('legacy'));
providers.testProviderPresent('default');

const hashes = getHashes();
const input = Buffer.alloc(0);
const md5 = 'd41d8cd98f00b204e9800998ecf8427e';
assert.strictEqual(createHash('md5').update(input).digest('hex'), md5);
assert.strictEqual(oneShotHash('md5', input), md5);

setFips(true);
assert.deepStrictEqual(getHashes(), []);
assert.throws(
  () => createHash('md5'),
  { code: 'ERR_OSSL_EVP_UNSUPPORTED' },
);
assert.throws(
  () => oneShotHash('md5', input),
  { code: 'ERR_OSSL_EVP_UNSUPPORTED' },
);

setFips(false);
assert.deepStrictEqual(getHashes(), hashes);
assert.strictEqual(createHash('md5').update(input).digest('hex'), md5);
assert.strictEqual(oneShotHash('md5', input), md5);

for (const hash of ['md4', 'whirlpool']) {
  assert(!hashes.includes(hash));
  assert.throws(() => createHash(hash), { code: 'ERR_OSSL_EVP_UNSUPPORTED' });
}

const worker = new Worker(`
  'use strict';
  const {
    createHash,
    getHashes,
    hash,
  } = require('node:crypto');
  const { parentPort } = require('node:worker_threads');

  const input = Buffer.alloc(0);
  const hashes = getHashes();
  const liveHash = createHash('md5').update(input);
  hash('md5', input);
  parentPort.postMessage({ phase: 'warm' });

  function getErrorCode(fn) {
    try {
      fn();
    } catch (err) {
      return err.code;
    }
  }

  parentPort.on('message', (phase) => {
    if (phase === 'fips-on') {
      parentPort.postMessage({
        phase,
        createHashError: getErrorCode(() => createHash('md5')),
        oneShotHashError: getErrorCode(() => hash('md5', input)),
        hashes: getHashes(),
        liveDigest: liveHash.digest('hex'),
      });
    } else {
      parentPort.postMessage({
        phase,
        createHashDigest: createHash('md5').update(input).digest('hex'),
        oneShotHashDigest: hash('md5', input),
        hashes: getHashes(),
      });
      parentPort.close();
    }
  });
`, { eval: true });

worker.once('message', common.mustCall((message) => {
  assert.strictEqual(message.phase, 'warm');
  setFips(true);

  worker.once('message', common.mustCall((message) => {
    assert.strictEqual(message.phase, 'fips-on');
    assert.strictEqual(message.createHashError, 'ERR_OSSL_EVP_UNSUPPORTED');
    assert.strictEqual(message.oneShotHashError, 'ERR_OSSL_EVP_UNSUPPORTED');
    assert.deepStrictEqual(message.hashes, []);
    assert.strictEqual(message.liveDigest, md5);

    setFips(false);

    worker.once('message', common.mustCall((message) => {
      assert.strictEqual(message.phase, 'fips-off');
      assert.strictEqual(message.createHashDigest, md5);
      assert.strictEqual(message.oneShotHashDigest, md5);
      assert.deepStrictEqual(message.hashes, hashes);
    }));
    worker.postMessage('fips-off');
  }));
  worker.postMessage('fips-on');
}));
worker.on('error', common.mustNotCall());
worker.on('exit', common.mustCall((code) => assert.strictEqual(code, 0)));
