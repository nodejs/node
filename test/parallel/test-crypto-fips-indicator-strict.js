'use strict';

const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
}

const {
  isBoringSSL,
  hasFIPS,
  hasOpenSSL,
} = require('../common/crypto');

if (isBoringSSL) {
  common.skip('BoringSSL does not support FIPS');
}

const assert = require('node:assert');
const diagnosticsChannel = require('node:diagnostics_channel');
const { once } = require('node:events');
const { createHmac, subtle } = require('node:crypto');
const { Worker } = require('node:worker_threads');
const {
  spawnSyncAndExitWithoutError,
} = require('../common/child_process');

const channelName = 'crypto.fips.indicator';
const mode = process.env.NODE_TEST_FIPS_FORCE_MODE;

if (!hasOpenSSL(3, 4)) {
  common.skip('OpenSSL 3.4 or later is required');
} else if (!hasFIPS(3, 4)) {
  common.skip('an active OpenSSL FIPS provider is required');
} else if (mode === 'provider') {
  assertSerializedMode(mode);
  assert.strictEqual(
    createHmac('sha256', Buffer.alloc(13)).digest().byteLength, 32);
} else if (mode === 'strict') {
  assertSerializedMode(mode);
  const subscriber = common.mustNotCall();
  diagnosticsChannel.subscribe(channelName, subscriber);
  assert.throws(
    () => createHmac('sha256', Buffer.alloc(13)).digest(),
    { code: /^ERR_OSSL_/ });
  setImmediate(common.mustCall(() => {
    assert.strictEqual(
      diagnosticsChannel.unsubscribe(channelName, subscriber), true);
  }));
} else if (mode === 'strict-events') {
  assertSerializedMode('strict');
  testStrictEvents().then(common.mustCall());
} else {
  runParent();
}

function assertSerializedMode(expected) {
  const { getOptionsAsFlagsFromBinding } = require('internal/options');
  assert.ok(getOptionsAsFlagsFromBinding().includes(`--force-fips=${expected}`));
}

function nextIndicator() {
  const keepAlive = setInterval(common.mustNotCall(), 10_000);
  const { promise, resolve } = Promise.withResolvers();
  const subscriber = common.mustCall((event, name) => {
    assert.strictEqual(name, channelName);
    clearInterval(keepAlive);
    assert.strictEqual(
      diagnosticsChannel.unsubscribe(channelName, subscriber), true);
    resolve(event);
  });
  diagnosticsChannel.subscribe(channelName, subscriber);
  return promise;
}

function runParent() {
  try {
    createHmac('sha256', Buffer.alloc(13)).digest();
  } catch (error) {
    assert.match(error.code, /^ERR_OSSL_/);
    common.printSkipMessage(
      'the FIPS provider rejects unapproved operations before signaling');
    return;
  }

  for (const [args, childMode] of [
    [['--force-fips'], 'provider'],
    [['--force-fips=provider'], 'provider'],
    [['--force-fips=strict'], 'strict'],
    [[
      '--force-fips=strict',
      '--enable-fips-indicator-events',
    ], 'strict-events'],
  ]) {
    spawnSyncAndExitWithoutError(
      process.execPath, [...args, '--expose-internals', __filename], {
        env: { ...process.env, NODE_TEST_FIPS_FORCE_MODE: childMode },
      });
  }
}

async function testStrictEvents() {
  const key = Buffer.alloc(13);

  assert.throws(
    () => createHmac('sha256', key).digest(),
    { code: /^ERR_OSSL_/ });

  let eventPromise = nextIndicator();
  assert.throws(
    () => createHmac('sha256', key).digest(),
    { code: /^ERR_OSSL_/ });
  assert.deepStrictEqual(await eventPromise, {
    operation: 'HMAC',
    reason: 'keysize',
    blocked: true,
    count: 1,
    dropped: 0,
  });

  const hmacKey = await subtle.importKey(
    'raw', key, { name: 'HMAC', hash: 'SHA-256' }, false, ['sign']);
  eventPromise = nextIndicator();
  await assert.rejects(
    subtle.sign('HMAC', hmacKey, Buffer.alloc(0)),
    { name: 'OperationError' });
  const webCryptoEvent = await eventPromise;
  assert.strictEqual(webCryptoEvent.operation, 'HMAC');
  assert.strictEqual(webCryptoEvent.reason, 'keysize');
  assert.strictEqual(webCryptoEvent.blocked, true);

  eventPromise = nextIndicator();
  const worker = new Worker(`
    'use strict';
    const { createHmac } = require('node:crypto');
    const { workerData } = require('node:worker_threads');
    createHmac('sha256', workerData.key).digest();
  `, {
    eval: true,
    workerData: { key },
  });
  const errorPromise = once(worker, 'error');
  const exitPromise = new Promise((resolve) => worker.on('exit', resolve));
  const [[error], workerEvent] = await Promise.all([
    errorPromise,
    eventPromise,
  ]);
  assert.match(error.code, /^ERR_OSSL_/);
  assert.deepStrictEqual(workerEvent, {
    operation: 'HMAC',
    reason: 'keysize',
    blocked: true,
    count: 1,
    dropped: 0,
  });
  const exitCode = await exitPromise;
  assert.strictEqual(exitCode, 1);
}
