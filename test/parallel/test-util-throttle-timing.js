// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('node:assert');
const { mock } = require('node:test');
const { setImmediate } = require('node:timers/promises');
const { internalBinding } = require('internal/test/binding');

// Throttle uses the libuv clock as well as timers. Control both before loading
// the implementation, which captures setTimeout and clearTimeout.
mock.timers.enable({ apis: ['Date', 'setTimeout'] });
mock.method(internalBinding('timers'), 'getLibuvNow', () => Date.now());
const { throttle } = require('node:util');

process.on('unhandledRejection', common.mustNotCall());

(async () => {
  {
    const values = [];
    const times = [];
    const start = Date.now();
    const throttled = throttle(common.mustCall(function(value) {
      assert.strictEqual(this, throttled);
      values.push(value);
      times.push(Date.now() - start);
      return value;
    }, 5), 2, 40);

    assert.strictEqual(typeof throttled.cancel, 'function');
    assert.strictEqual(typeof throttled.hasImmediateCapacity, 'function');
    assert.strictEqual(typeof throttled.ref, 'function');
    assert.strictEqual(typeof throttled.unref, 'function');
    assert.strictEqual(throttled.pending, null);
    assert.strictEqual(throttled.pendingCount, 0);
    assert.strictEqual(throttled.activeCount, 0);
    assert.strictEqual(throttled.hasImmediateCapacity(), true);

    const first = throttled(1);
    const second = throttled(2);
    const third = throttled(3);
    const fourth = throttled(4);
    const fifth = throttled(5);

    assert(first instanceof Promise);
    assert(second instanceof Promise);
    assert.notStrictEqual(first, second);
    assert.deepStrictEqual(values, [1, 2]);
    assert.strictEqual(throttled.hasImmediateCapacity(), false);
    assert.strictEqual(throttled.pending, fifth);
    assert.strictEqual(throttled.pendingCount, 3);
    assert.strictEqual(throttled.unref(), throttled);
    assert.strictEqual(throttled.ref(), throttled);

    mock.timers.tick(39);
    assert.deepStrictEqual(values, [1, 2]);
    mock.timers.tick(1);
    assert.deepStrictEqual(values, [1, 2, 3, 4]);
    assert.strictEqual(throttled.pendingCount, 1);
    mock.timers.tick(39);
    assert.deepStrictEqual(values, [1, 2, 3, 4]);
    mock.timers.tick(1);

    assert.deepStrictEqual(
      await Promise.all([first, second, third, fourth, fifth]),
      [1, 2, 3, 4, 5],
    );
    assert.deepStrictEqual(values, [1, 2, 3, 4, 5]);
    assert.strictEqual(throttled.pending, null);
    assert.strictEqual(throttled.pendingCount, 0);
    assert.strictEqual(throttled.activeCount, 0);
    assert.deepStrictEqual(times, [0, 0, 40, 40, 80]);
  }

  {
    const values = [];
    const throttled = throttle(common.mustCall((value) => {
      values.push(value);
      return value;
    }, 2), 1, 20, { maxPending: 1 });
    const first = throttled(1);
    const second = throttled(2);
    const dropped = throttled(3);

    assert.deepStrictEqual(values, [1]);
    assert.strictEqual(throttled.pendingCount, 1);
    await setImmediate();
    await assert.rejects(dropped, { code: 'ERR_THROTTLED' });
    mock.timers.tick(19);
    assert.deepStrictEqual(values, [1]);
    mock.timers.tick(1);
    assert.deepStrictEqual(await Promise.all([first, second]), [1, 2]);
    assert.deepStrictEqual(values, [1, 2]);
  }

  {
    const values = [];
    const throttled = throttle(common.mustCall((value) => {
      values.push(value);
      return value;
    }, 3), 2, 30, { overflow: 'drop' });
    const first = throttled(1);
    const second = throttled(2);
    await assert.rejects(throttled(3), { code: 'ERR_THROTTLED' });
    assert.deepStrictEqual(await Promise.all([first, second]), [1, 2]);

    mock.timers.tick(29);
    assert.strictEqual(throttled.hasImmediateCapacity(), false);
    await assert.rejects(throttled(4), { code: 'ERR_THROTTLED' });
    mock.timers.tick(1);
    assert.strictEqual(throttled.hasImmediateCapacity(), true);
    assert.strictEqual(await throttled(5), 5);
    assert.deepStrictEqual(values, [1, 2, 5]);
  }

  // A rolling window releases one slot at a time when timers run on schedule.
  // If dispatch is delayed until both slots expire, both calls may run together.
  for (const delayed of [false, true]) {
    const times = [];
    const start = Date.now();
    const throttled = throttle(common.mustCall((value) => {
      times.push(Date.now() - start);
      return value;
    }, 4), 2, 80, { strict: true });

    const first = throttled(1);
    mock.timers.tick(40);
    const second = throttled(2);
    const third = throttled(3);
    const fourth = throttled(4);

    mock.timers.tick(39);
    assert.deepStrictEqual(times, [0, 40]);
    if (delayed) {
      mock.timers.tick(41);
    } else {
      mock.timers.tick(1);
      assert.deepStrictEqual(times, [0, 40, 80]);
      mock.timers.tick(39);
      assert.deepStrictEqual(times, [0, 40, 80]);
      mock.timers.tick(1);
    }

    assert.deepStrictEqual(
      await Promise.all([first, second, third, fourth]),
      [1, 2, 3, 4],
    );
    assert.deepStrictEqual(times, delayed ? [0, 40, 120, 120] : [0, 40, 80, 120]);
  }

  {
    let recursive;
    const values = [];
    const throttled = throttle(common.mustCall((value) => {
      values.push(value);
      if (value === 1) recursive = throttled(2);
      return value;
    }, 2), 1, 20);

    const first = throttled(1);
    assert.deepStrictEqual(values, [1]);
    assert.strictEqual(throttled.pending, recursive);
    assert.strictEqual(throttled.pendingCount, 1);
    mock.timers.tick(19);
    assert.deepStrictEqual(values, [1]);
    mock.timers.tick(1);
    assert.deepStrictEqual(await Promise.all([first, recursive]), [1, 2]);
    assert.deepStrictEqual(values, [1, 2]);
  }
})().then(common.mustCall()).finally(() => mock.reset());
