'use strict';
const common = require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../common/tmpdir');

// Test that coroutine-based fs bindings integrate with async_hooks
// and AsyncLocalStorage via UvTrackedTask.

const binding = process.binding('fs');

tmpdir.refresh();

// ===========================================================================
// Test 1: AsyncLocalStorage context propagates through coroutines.
// ===========================================================================
{
  const als = new async_hooks.AsyncLocalStorage();
  const testFile = path.join(tmpdir.path, 'coro-als-test.txt');
  fs.writeFileSync(testFile, 'als-test-data');

  als.run('my-context-value', common.mustCall(() => {
    assert.strictEqual(als.getStore(), 'my-context-value');

    binding.coroAccess(testFile, fs.constants.R_OK)
      .then(common.mustCall(() => {
        // After the coroutine completes and the promise resolves,
        // the ALS context should still be available via V8's
        // ContinuationPreservedEmbedderData.
        assert.strictEqual(als.getStore(), 'my-context-value');
      }));
  }));
}

// ===========================================================================
// Test 2: AsyncLocalStorage propagates through multi-step coroutine.
// ===========================================================================
{
  const als = new async_hooks.AsyncLocalStorage();
  const testFile = path.join(tmpdir.path, 'coro-als-readfile-test.txt');
  fs.writeFileSync(testFile, 'multi-step-als-data');

  als.run({ requestId: 42 }, common.mustCall(() => {
    assert.deepStrictEqual(als.getStore(), { requestId: 42 });

    binding.coroReadFileBytes(testFile)
      .then(common.mustCall((buf) => {
        // Even after open->stat->read->close, ALS context is preserved.
        assert.deepStrictEqual(als.getStore(), { requestId: 42 });
        assert.strictEqual(buf.toString(), 'multi-step-als-data');
      }));
  }));
}

// ===========================================================================
// Test 3: Different ALS contexts in concurrent coroutines stay isolated.
// ===========================================================================
{
  const als = new async_hooks.AsyncLocalStorage();
  const file1 = path.join(tmpdir.path, 'coro-als-concurrent-1.txt');
  const file2 = path.join(tmpdir.path, 'coro-als-concurrent-2.txt');
  fs.writeFileSync(file1, 'data-one');
  fs.writeFileSync(file2, 'data-two');

  als.run('context-A', common.mustCall(() => {
    binding.coroReadFileBytes(file1)
      .then(common.mustCall((buf) => {
        assert.strictEqual(als.getStore(), 'context-A');
        assert.strictEqual(buf.toString(), 'data-one');
      }));
  }));

  als.run('context-B', common.mustCall(() => {
    binding.coroReadFileBytes(file2)
      .then(common.mustCall((buf) => {
        assert.strictEqual(als.getStore(), 'context-B');
        assert.strictEqual(buf.toString(), 'data-two');
      }));
  }));
}

// ===========================================================================
// Test 4: async_hooks init/before/after/destroy fire for UvTrackedTask.
// ===========================================================================
{
  const events = [];
  const testFile = path.join(tmpdir.path, 'coro-hooks-test.txt');
  fs.writeFileSync(testFile, 'hooks-test');

  // Find the FSREQPROMISE async resource from the coroutine binding.
  let trackedTaskId = null;

  const hook = async_hooks.createHook({
    init(asyncId, type, triggerAsyncId) {
      if (type === 'FSREQPROMISE' && trackedTaskId === null) {
        trackedTaskId = asyncId;
        events.push({ event: 'init', asyncId, type, triggerAsyncId });
      }
    },
    before(asyncId) {
      if (asyncId === trackedTaskId) {
        events.push({ event: 'before', asyncId });
      }
    },
    after(asyncId) {
      if (asyncId === trackedTaskId) {
        events.push({ event: 'after', asyncId });
      }
    },
    destroy(asyncId) {
      if (asyncId === trackedTaskId) {
        events.push({ event: 'destroy', asyncId });
      }
    },
  });

  hook.enable();

  binding.coroAccess(testFile, fs.constants.R_OK)
    .then(common.mustCall(() => {
      // Allow destroy to be emitted (it's batched via SetImmediate,
      // so we need a couple of ticks for it to flush).
      setImmediate(common.mustCall(() => setImmediate(common.mustCall(() => {
        hook.disable();

        assert.ok(trackedTaskId !== null,
                  'should have seen init for FSREQPROMISE');

        const eventTypes = events.map((e) => e.event);

        assert.ok(eventTypes.includes('init'));
        assert.ok(eventTypes.includes('before'));
        assert.ok(eventTypes.includes('after'));
        assert.ok(eventTypes.includes('destroy'));

        // Init should be first.
        assert.strictEqual(eventTypes[0], 'init');

        // Before should come before after.
        const firstBefore = eventTypes.indexOf('before');
        const firstAfter = eventTypes.indexOf('after');
        assert.ok(firstBefore < firstAfter);
      }))));
    }));
}

// ===========================================================================
// Test 5: Multi-step coroutine fires multiple before/after pairs.
// ===========================================================================
{
  const events = [];
  const testFile = path.join(tmpdir.path, 'coro-hooks-multi-test.txt');
  fs.writeFileSync(testFile, 'multi-hooks-test');

  let trackedTaskId = null;

  const hook = async_hooks.createHook({
    init(asyncId, type) {
      if (type === 'COROREADFILE' && trackedTaskId === null) {
        trackedTaskId = asyncId;
      }
    },
    before(asyncId) {
      if (asyncId === trackedTaskId) {
        events.push('before');
      }
    },
    after(asyncId) {
      if (asyncId === trackedTaskId) {
        events.push('after');
      }
    },
    destroy(asyncId) {
      if (asyncId === trackedTaskId) {
        events.push('destroy');
      }
    },
  });

  hook.enable();

  // CoroReadFileBytes does open->stat->read->close = 4 libuv ops.
  // Each should produce a before/after pair.
  binding.coroReadFileBytes(testFile)
    .then(common.mustCall((buf) => {
      assert.strictEqual(buf.toString(), 'multi-hooks-test');

      setImmediate(common.mustCall(() => setImmediate(common.mustCall(() => {
        hook.disable();

        const beforeCount = events.filter((e) => e === 'before').length;
        const afterCount = events.filter((e) => e === 'after').length;

        // Open + stat + read + close = at least 4 before/after pairs
        // (could be 5 if the read loop does an extra read to hit EOF).
        assert.ok(beforeCount >= 4,
                  `expected >= 4 before events, got ${beforeCount}`);
        assert.strictEqual(beforeCount, afterCount);

        const destroyCount = events.filter((e) => e === 'destroy').length;
        assert.strictEqual(destroyCount, 1);
      }))));
    }));
}

// ===========================================================================
// Test 6: Different coroutine bindings report distinct type strings.
// ===========================================================================
{
  const types = new Set();
  const testFile = path.join(tmpdir.path, 'coro-hooks-types-test.txt');
  fs.writeFileSync(testFile, 'type-test');

  const hook = async_hooks.createHook({
    init(asyncId, type) {
      if (type === 'FSREQPROMISE' || type === 'COROREADFILE') {
        types.add(type);
      }
    },
  });

  hook.enable();

  Promise.all([
    binding.coroAccess(testFile, fs.constants.R_OK),
    binding.coroReadFileBytes(testFile),
  ]).then(common.mustCall(() => {
    hook.disable();

    // Both types should appear — they are distinct.
    assert.ok(types.has('FSREQPROMISE'));
    assert.ok(types.has('COROREADFILE'));
    assert.strictEqual(types.size, 2);
  }));
}
