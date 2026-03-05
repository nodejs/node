'use strict';

// Flags: --expose-internals

const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
}

const assert = require('node:assert');
const diagnosticsChannel = require('node:diagnostics_channel');
const { once } = require('node:events');
const { Worker } = require('node:worker_threads');
const {
  spawnSyncAndExitWithoutError,
} = require('../common/child_process');
const { hasFIPS, hasOpenSSL } = require('../common/crypto');
const {
  createHmac,
  generateKeyPairSync,
  sign,
  subtle,
} = require('node:crypto');

const channelName = 'crypto.fips.indicator';

if (!hasOpenSSL(3, 4)) {
  common.skip('OpenSSL 3.4 or later is required');
} else if (!hasFIPS(3, 4)) {
  common.skip('an active OpenSSL 3.4+ FIPS provider is required');
} else if (!process.execArgv.includes('--enable-fips-indicator-events')) {
  spawnSyncAndExitWithoutError(
    process.execPath,
    ['--enable-fips-indicator-events', '--expose-internals', __filename],
  );
} else {
  run().then(common.mustCall());
}

async function drainTicks() {
  for (let i = 0; i < 3; i++) {
    await new Promise((resolve) => setImmediate(resolve));
  }
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

function testUnsubscribeDuringDrain(key, privateKey) {
  const keepAlive = setInterval(common.mustNotCall(), 10_000);
  const { promise, resolve } = Promise.withResolvers();
  const subscriber = common.mustCall(() => {
    clearInterval(keepAlive);
    assert.strictEqual(
      diagnosticsChannel.unsubscribe(channelName, subscriber), true);
    setImmediate(common.mustCall(resolve));
  });
  diagnosticsChannel.subscribe(channelName, subscriber);
  createHmac('sha256', key).digest();
  sign('sha1', Buffer.alloc(0), privateKey);
  return promise;
}

async function testNoStaleIndicator(key) {
  createHmac('sha256', key).digest();
  const subscriber = common.mustNotCall();
  diagnosticsChannel.subscribe(channelName, subscriber);
  await new Promise((resolve) => setImmediate(resolve));
  assert.strictEqual(
    diagnosticsChannel.unsubscribe(channelName, subscriber), true);
}

async function run() {
  const key = Buffer.alloc(13);

  const keepAlive = setInterval(common.mustNotCall(), 10_000);
  const { promise, resolve: resolveProbe } = Promise.withResolvers();
  const probeSubscriber = (event) => {
    clearInterval(keepAlive);
    diagnosticsChannel.unsubscribe(channelName, probeSubscriber);
    resolveProbe(event);
  };
  diagnosticsChannel.subscribe(channelName, probeSubscriber);

  let output;
  try {
    output = createHmac('sha256', key).digest();
  } catch (error) {
    clearInterval(keepAlive);
    diagnosticsChannel.unsubscribe(channelName, probeSubscriber);
    assert.match(error.code, /^ERR_OSSL_/);
    common.printSkipMessage(
      'the FIPS provider rejects unapproved operations before signaling');
    return;
  }

  assert.strictEqual(output.byteLength, 32);
  assert.deepStrictEqual(await promise, {
    operation: 'HMAC',
    reason: 'keysize',
    blocked: false,
    count: 1,
    dropped: 0,
  });

  await testNoStaleIndicator(key);

  let eventPromise = nextIndicator();
  createHmac('sha256', key).digest();
  createHmac('sha256', key).digest();
  assert.deepStrictEqual(await eventPromise, {
    operation: 'HMAC',
    reason: 'keysize',
    blocked: false,
    count: 2,
    dropped: 0,
  });

  const secondSubscriber = common.mustCall();
  diagnosticsChannel.subscribe(channelName, secondSubscriber);
  eventPromise = nextIndicator();
  createHmac('sha256', key).digest();
  await eventPromise;
  assert.strictEqual(
    diagnosticsChannel.unsubscribe(channelName, secondSubscriber), true);

  const { privateKey } = generateKeyPairSync('rsa', { modulusLength: 2048 });
  await testUnsubscribeDuringDrain(key, privateKey);

  const hmacKey = await subtle.importKey(
    'raw', Buffer.alloc(13), { name: 'HMAC', hash: 'SHA-256' }, false, ['sign']);
  eventPromise = nextIndicator();
  const [signature, hmacEvent] = await Promise.all([
    subtle.sign('HMAC', hmacKey, Buffer.alloc(0)),
    eventPromise,
  ]);
  assert.strictEqual(signature.byteLength, 32);
  assert.strictEqual(hmacEvent.operation, 'HMAC');
  assert.strictEqual(hmacEvent.reason, 'keysize');
  assert.strictEqual(hmacEvent.blocked, false);

  eventPromise = nextIndicator();
  const worker = new Worker(`
    'use strict';
    const diagnosticsChannel = require('node:diagnostics_channel');
    const { parentPort, workerData } = require('node:worker_threads');
    const { createHmac } = require('node:crypto');

    let indicatorCount = 0;
    diagnosticsChannel.subscribe('crypto.fips.indicator', () => {
      indicatorCount++;
    });
    const result = createHmac('sha256', workerData.key).digest();
    setImmediate(() => {
      parentPort.postMessage({
        indicatorCount,
        length: result.byteLength,
      });
    });
  `, {
    eval: true,
    workerData: { key },
  });
  worker.on('error', common.mustNotCall());
  const exitPromise = once(worker, 'exit');
  const [[message], workerEvent] = await Promise.all([
    once(worker, 'message'),
    eventPromise,
  ]);
  assert.deepStrictEqual(message, { indicatorCount: 0, length: 32 });
  assert.deepStrictEqual(workerEvent, {
    operation: 'HMAC',
    reason: 'keysize',
    blocked: false,
    count: 1,
    dropped: 0,
  });
  const [exitCode] = await exitPromise;
  assert.strictEqual(exitCode, 0);

  // A simulated attached tracer (forced probe semaphore) must not replay
  // queued indicator events into a subscriber that arrived later, and
  // pending events must survive removing the last subscriber while the
  // tracer stays attached.
  {
    const { internalBinding } = require('internal/test/binding');
    const { probeSemaphore } = internalBinding('diagnostics_channel');
    if (probeSemaphore !== undefined) {
      const initial = probeSemaphore[0];
      probeSemaphore[0] = 1;
      try {
        // Tracer-only event: queued while no subscriber exists.
        createHmac('sha256', key).digest();
        const lateSubscriber = common.mustNotCall();
        diagnosticsChannel.subscribe(channelName, lateSubscriber);
        await drainTicks();
        assert.strictEqual(
          diagnosticsChannel.unsubscribe(channelName, lateSubscriber), true);

        // Queued for a subscriber, then the last subscriber is removed
        // while the tracer stays attached.
        const seen = [];
        const first = (event) => seen.push(`first:${event.count}`);
        diagnosticsChannel.subscribe(channelName, first);
        createHmac('sha256', key).digest();
        assert.strictEqual(
          diagnosticsChannel.unsubscribe(channelName, first), true);

        const second = (event) => seen.push(`second:${event.count}`);
        diagnosticsChannel.subscribe(channelName, second);
        await drainTicks();
        assert.deepStrictEqual(seen, []);

        // Only a new event reaches the new subscriber, not coalesced with
        // the retained pre-unsubscribe event.
        createHmac('sha256', key).digest();
        await drainTicks();
        assert.deepStrictEqual(seen, ['second:1']);
        assert.strictEqual(
          diagnosticsChannel.unsubscribe(channelName, second), true);
      } finally {
        probeSemaphore[0] = initial;
      }
    }
  }
}
