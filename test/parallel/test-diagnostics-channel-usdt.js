// Flags: --expose-internals
'use strict';

// Verify that diagnostics channel publish works correctly with USDT probe
// code in the publish path, and that the probe semaphore and emitPublishProbe
// binding are wired up correctly.

const common = require('../common');
const dc = require('diagnostics_channel');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');

const binding = internalBinding('diagnostics_channel');

// --- Semaphore and binding shape ---

// probeSemaphore must be a Uint16Array (USDT compiled in) or undefined (not).
{
  const { probeSemaphore } = binding;
  assert.ok(
    probeSemaphore === undefined || probeSemaphore instanceof Uint16Array,
    `Expected probeSemaphore to be Uint16Array or undefined, got ${typeof probeSemaphore}`,
  );

  if (probeSemaphore !== undefined) {
    // Without a tracer attached the semaphore must be 0 (Linux, committed
    // SystemTap-generated header) or 1 (macOS --with-dtrace path, which
    // has no native semaphore).
    assert.ok(
      probeSemaphore[0] === 0 || probeSemaphore[0] === 1,
      `Expected semaphore to be 0 or 1, got ${probeSemaphore[0]}`,
    );

    // emitPublishProbe must exist when USDT is compiled in.
    assert.strictEqual(typeof binding.emitPublishProbe, 'function');
  } else {
    // emitPublishProbe must not exist when USDT is absent.
    assert.strictEqual(binding.emitPublishProbe, undefined);
  }
}

// --- JS probe guard: verify emitPublishProbe is called/skipped ---

// When the semaphore is > 0 (macOS --with-dtrace, which has no native
// semaphore), emitPublishProbe must be called for string-named channels and
// must NOT be called for symbol-named channels.  When the semaphore is 0
// (Linux Tier 1, no tracer) or USDT is absent, emitPublishProbe must never
// be called.
{
  const { probeSemaphore, emitPublishProbe } = binding;
  const semaphoreEnabled = probeSemaphore !== undefined &&
                           probeSemaphore[0] > 0;

  let probeCallCount = 0;
  const origProbe = emitPublishProbe;
  if (origProbe !== undefined) {
    binding.emitPublishProbe = (...args) => {
      probeCallCount++;
      return origProbe(...args);
    };
  }

  // String-named channel with subscriber — probe fires only if semaphore > 0.
  const ch = dc.channel('test:usdt:probe-guard');
  const subscriber = common.mustCall();
  ch.subscribe(subscriber);
  ch.publish({ probeGuard: true });
  ch.unsubscribe(subscriber);

  if (semaphoreEnabled) {
    assert.strictEqual(probeCallCount, 1,
                       `emitPublishProbe should be called once for ` +
                       `string-named channel, got ${probeCallCount}`);
  } else {
    assert.strictEqual(probeCallCount, 0,
                       `emitPublishProbe should not be called when the ` +
                       `semaphore is 0, got ${probeCallCount}`);
  }

  // Symbol-named channel — probe must never fire regardless of semaphore.
  probeCallCount = 0;
  const sym = Symbol('test:usdt:symbol-probe-guard');
  const symCh = dc.channel(sym);
  const symSub = common.mustCall();
  symCh.subscribe(symSub);
  symCh.publish({ symbolGuard: true });
  symCh.unsubscribe(symSub);

  assert.strictEqual(probeCallCount, 0,
                     `emitPublishProbe must not be called for symbol-named ` +
                     `channels, got ${probeCallCount}`);

  // Restore original.
  if (origProbe !== undefined) {
    binding.emitPublishProbe = origProbe;
  }
}

// --- JS probe guard: positive path through the public publish() API ---

// Force the semaphore to look "attached" and verify that the hot path in
// lib/internal/diagnostics_channel actually calls emitPublishProbe.  This
// exercises the module-level view of the semaphore (which must survive
// startup-snapshot deserialization) rather than the binding directly.
if (binding.probeSemaphore !== undefined) {
  const { probeSemaphore } = binding;
  const origSemaphore = probeSemaphore[0];
  let probeCalls = [];
  const origProbe = binding.emitPublishProbe;
  binding.emitPublishProbe = (name) => probeCalls.push(name);

  try {
    probeSemaphore[0] = 1;
    const ch = dc.channel('test:usdt:probe-positive');
    const subscriber = common.mustCall();
    ch.subscribe(subscriber);
    probeCalls = [];
    ch.publish({ probePositive: true });
    ch.unsubscribe(subscriber);

    assert.deepStrictEqual(probeCalls, ['test:usdt:probe-positive'],
                           `publish() must call emitPublishProbe for string-named channels ` +
      `when the semaphore is > 0, got ${JSON.stringify(probeCalls)}`);

    // Symbol-named channels must never emit the probe.
    probeCalls = [];
    const sym = Symbol('test:usdt:symbol-probe-positive');
    const symCh = dc.channel(sym);
    const symSub = common.mustCall();
    symCh.subscribe(symSub);
    symCh.publish({ symbolPositive: true });
    symCh.unsubscribe(symSub);

    assert.deepStrictEqual(probeCalls, [],
                           `publish() must not call emitPublishProbe for symbol-named ` +
      `channels, got ${JSON.stringify(probeCalls)}`);

    // With the semaphore back at 0, the probe must not be emitted.
    probeSemaphore[0] = 0;
    probeCalls = [];
    const ch0 = dc.channel('test:usdt:probe-negative');
    const sub0 = common.mustCall();
    ch0.subscribe(sub0);
    ch0.publish({ probeNegative: true });
    ch0.unsubscribe(sub0);

    assert.deepStrictEqual(probeCalls, [],
                           `publish() must not call emitPublishProbe when the semaphore is 0, ` +
      `got ${JSON.stringify(probeCalls)}`);
  } finally {
    probeSemaphore[0] = origSemaphore;
    binding.emitPublishProbe = origProbe;
  }
}

// --- Publish with and without subscribers ---

// String-named channel with subscribers.
{
  const ch = dc.channel('test:usdt:string');
  const input = { foo: 'bar' };

  const subscriber = common.mustCall((message, name) => {
    assert.strictEqual(name, 'test:usdt:string');
    assert.deepStrictEqual(message, input);
  });

  ch.subscribe(subscriber);
  assert.ok(ch.hasSubscribers);
  ch.publish(input);
  ch.unsubscribe(subscriber);
}

// String-named channel without subscribers (exercises the C++
// Channel::Publish early-return / fire_usdt_probe path).
{
  const ch = dc.channel('test:usdt:no-sub');
  assert.ok(!ch.hasSubscribers);
  ch.publish({ data: 1 });
}

// Symbol-named channel with subscribers.
{
  const sym = Symbol('test:usdt:symbol');
  const ch = dc.channel(sym);
  const input = { baz: 'qux' };

  const subscriber = common.mustCall((message, name) => {
    assert.strictEqual(name, sym);
    assert.deepStrictEqual(message, input);
  });

  ch.subscribe(subscriber);
  assert.ok(ch.hasSubscribers);
  ch.publish(input);
  ch.unsubscribe(subscriber);
}

// Symbol-named channel without subscribers.
{
  const sym = Symbol('test:usdt:symbol-nosub');
  const ch = dc.channel(sym);
  assert.ok(!ch.hasSubscribers);
  ch.publish({ data: 2 });
}

// --- Non-object messages (nullptr branch in EmitPublishProbe) ---

{
  const ch = dc.channel('test:usdt:primitive');
  const received = [];
  const subscriber = common.mustCall((message) => {
    received.push(message);
  }, 4);

  ch.subscribe(subscriber);
  ch.publish('hello');
  ch.publish(42);
  ch.publish(null);
  ch.publish(undefined);
  ch.unsubscribe(subscriber);

  assert.deepStrictEqual(received, ['hello', 42, null, undefined]);
}

// --- Active-to-inactive lifecycle ---

// Publish after unsubscribe: channel reverts to inactive, publish must still
// work (hits the no-subscriber early-return with fire_usdt_probe in C++).
{
  const ch = dc.channel('test:usdt:lifecycle');
  const subscriber = common.mustCall((message) => {
    assert.deepStrictEqual(message, { step: 1 });
  });

  ch.subscribe(subscriber);
  ch.publish({ step: 1 });
  ch.unsubscribe(subscriber);
  assert.ok(!ch.hasSubscribers);
  ch.publish({ step: 2 });
}

// Re-subscribe after unsubscribe: verifies the channel transitions back to
// active correctly and the probe path still works.
{
  const ch = dc.channel('test:usdt:resubscribe');
  const first = common.mustCall();
  ch.subscribe(first);
  ch.publish({ phase: 'first' });
  ch.unsubscribe(first);

  assert.ok(!ch.hasSubscribers);
  ch.publish({ phase: 'inactive' });

  const second = common.mustCall((message) => {
    assert.deepStrictEqual(message, { phase: 'second' });
  });
  ch.subscribe(second);
  assert.ok(ch.hasSubscribers);
  ch.publish({ phase: 'second' });
  ch.unsubscribe(second);
}

// --- Direct emitPublishProbe call (when available) ---

// Call emitPublishProbe directly to exercise the C++ function with various
// argument types.  This path is normally guarded by the semaphore in JS,
// so it may not be reached in normal testing.
// NOTE: On Tier 1 (dtrace -h) builds without a tracer attached,
// NODE_DC_PUBLISH_ENABLED() returns false and the probe body is skipped.
// Full probe exercising requires the bpftrace integration test.
{
  const { probeSemaphore, emitPublishProbe } = binding;
  if (probeSemaphore !== undefined && emitPublishProbe !== undefined) {
    // Object message.
    emitPublishProbe('test:usdt:direct', { x: 1 });
    // Non-object message (nullptr branch).
    emitPublishProbe('test:usdt:direct', 'string');
    emitPublishProbe('test:usdt:direct', null);
    emitPublishProbe('test:usdt:direct', 42);
    emitPublishProbe('test:usdt:direct', undefined);
  }
}
