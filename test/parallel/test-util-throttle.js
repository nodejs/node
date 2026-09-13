// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('node:assert');
const { createHook } = require('node:async_hooks');
const { setImmediate, setTimeout } = require('node:timers/promises');
const { throttle } = require('node:util');
const { TIMEOUT_MAX } = require('internal/timers');

process.on('unhandledRejection', common.mustNotCall());

for (const value of [undefined, null, true, 0, 'fn', {}, [], Symbol()]) {
  assert.throws(() => throttle(value, 1, 1), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
}

for (const value of [undefined, null, true, '1', {}, [], Symbol()]) {
  assert.throws(() => throttle(() => {}, value, 1), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
}

for (const value of [-1, 0, 0.5, NaN, Infinity]) {
  assert.throws(() => throttle(() => {}, value, 1), {
    code: 'ERR_OUT_OF_RANGE',
  });
}

for (const value of [undefined, null, true, '1', {}, [], Symbol()]) {
  assert.throws(() => throttle(() => {}, 1, value), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
}

for (const value of [-1, 0.5, NaN, Infinity, TIMEOUT_MAX + 1]) {
  assert.throws(() => throttle(() => {}, 1, value), {
    code: 'ERR_OUT_OF_RANGE',
  });
}

for (const value of [null, true, 1, 'options', []]) {
  assert.throws(() => throttle(() => {}, 1, 1, value), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
}

assert.throws(() => throttle(() => {}, 1, 1, { signal: {} }), {
  code: 'ERR_INVALID_ARG_TYPE',
});
assert.throws(() => throttle(() => {}, 1, 1, { strict: 1 }), {
  code: 'ERR_INVALID_ARG_TYPE',
});

for (const value of [null, true, 0, 'invalid', {}, []]) {
  assert.throws(() => throttle(() => {}, 1, 1, { overflow: value }), {
    code: 'ERR_INVALID_ARG_VALUE',
  });
}

for (const option of ['concurrency', 'maxPending']) {
  for (const value of [null, true, '1', {}, [], Symbol()]) {
    assert.throws(() => throttle(() => {}, 1, 1, { [option]: value }), {
      code: 'ERR_INVALID_ARG_TYPE',
    });
  }
}

for (const value of [-1, 0, 0.5, NaN, -Infinity]) {
  assert.throws(() => throttle(() => {}, 1, 1, { concurrency: value }), {
    code: 'ERR_OUT_OF_RANGE',
  });
}

for (const value of [-1, 0.5, NaN, -Infinity]) {
  assert.throws(() => throttle(() => {}, 1, 1, { maxPending: value }), {
    code: 'ERR_OUT_OF_RANGE',
  });
}

// Explicit Infinity values are accepted.
throttle(() => {}, 1, 1, {
  concurrency: Infinity,
  maxPending: Infinity,
});

{
  const reason = new Error('already aborted');
  assert.throws(
    () => throttle(() => {}, 1, 1, {
      signal: AbortSignal.abort(reason),
    }),
    (error) => error.code === 'ABORT_ERR' && error.cause === reason,
  );
}

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

    assert.deepStrictEqual(
      await Promise.all([first, second, third, fourth, fifth]),
      [1, 2, 3, 4, 5],
    );
    assert.deepStrictEqual(values, [1, 2, 3, 4, 5]);
    assert.strictEqual(throttled.pending, null);
    assert.strictEqual(throttled.pendingCount, 0);
    assert.strictEqual(throttled.activeCount, 0);
    assert(times[2] - times[0] >= 30);
    assert(times[4] - times[2] >= 30);
  }

  {
    let running = 0;
    let maxRunning = 0;
    let timeoutCount = 0;
    const releases = [];
    const values = [];
    const hook = createHook({
      init(_asyncId, type) {
        if (type === 'Timeout') timeoutCount++;
      },
    });
    const throttled = throttle(common.mustCall((value) => {
      running++;
      maxRunning = Math.max(maxRunning, running);
      values.push(value);
      const { promise, resolve } = Promise.withResolvers();
      releases.push(() => {
        running--;
        resolve(value);
      });
      return promise;
    }, 4), 10, 1_000, { concurrency: 2 });

    hook.enable();
    const calls = [
      throttled(1),
      throttled(2),
      throttled(3),
      throttled(4),
    ];
    hook.disable();

    assert.deepStrictEqual(values, [1, 2]);
    assert.strictEqual(throttled.hasImmediateCapacity(), false);
    assert.strictEqual(throttled.activeCount, 2);
    assert.strictEqual(throttled.pendingCount, 2);
    assert.strictEqual(timeoutCount, 0);

    releases[0]();
    await setImmediate();
    assert.deepStrictEqual(values, [1, 2, 3]);
    assert.strictEqual(throttled.activeCount, 2);
    assert.strictEqual(throttled.pendingCount, 1);

    releases[1]();
    await setImmediate();
    assert.deepStrictEqual(values, [1, 2, 3, 4]);
    assert.strictEqual(throttled.activeCount, 2);
    assert.strictEqual(throttled.pendingCount, 0);

    releases[2]();
    releases[3]();
    assert.deepStrictEqual(await Promise.all(calls), [1, 2, 3, 4]);
    assert.strictEqual(throttled.activeCount, 0);
    assert.strictEqual(maxRunning, 2);
  }

  {
    const deferred = Promise.withResolvers();
    const throttled = throttle(common.mustCall(() => deferred.promise), 10, 100, {
      concurrency: 1,
      overflow: 'drop',
    });
    const first = throttled();
    const dropped = throttled();

    assert.strictEqual(throttled.activeCount, 1);
    assert.strictEqual(throttled.pendingCount, 0);
    assert.strictEqual(throttled.hasImmediateCapacity(), false);
    await setImmediate();
    await assert.rejects(dropped, { code: 'ERR_THROTTLED' });
    deferred.resolve('result');
    assert.strictEqual(await first, 'result');
    assert.strictEqual(throttled.activeCount, 0);
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
    assert.deepStrictEqual(await Promise.all([first, second]), [1, 2]);
    assert.deepStrictEqual(values, [1, 2]);
  }

  {
    let timeoutCount = 0;
    const hook = createHook({
      init(_asyncId, type) {
        if (type === 'Timeout') timeoutCount++;
      },
    });
    const values = [];
    const throttled = throttle(common.mustCall((value) => {
      values.push(value);
      return value;
    }, 3), 2, 30, { overflow: 'drop' });

    hook.enable();
    const first = throttled(1);
    const second = throttled(2);
    const dropped = throttled(3);
    hook.disable();

    assert.deepStrictEqual(values, [1, 2]);
    assert.strictEqual(timeoutCount, 0);
    assert.strictEqual(throttled.pending, null);
    assert.strictEqual(throttled.pendingCount, 0);
    await setImmediate();
    await assert.rejects(dropped, { code: 'ERR_THROTTLED' });
    assert.deepStrictEqual(await Promise.all([first, second]), [1, 2]);

    await setTimeout(30);
    assert.strictEqual(await throttled(4), 4);
    assert.deepStrictEqual(values, [1, 2, 4]);
  }

  {
    const times = [];
    const start = Date.now();
    const throttled = throttle(common.mustCall((value) => {
      times.push(Date.now() - start);
      return value;
    }, 4), 2, 80, { strict: true });

    const first = throttled(1);
    await setTimeout(40);
    const second = throttled(2);
    const third = throttled(3);
    const fourth = throttled(4);

    assert.deepStrictEqual(
      await Promise.all([first, second, third, fourth]),
      [1, 2, 3, 4],
    );
    assert(times[2] - times[0] >= 65);
    assert(times[3] - times[1] >= 65);
    assert(times[3] - times[2] >= 25);
  }

  {
    const reason = new Error('cancelled');
    const throttled = throttle(common.mustCall((value) => value, 2), 1, 100);
    const first = throttled(1);
    const second = throttled(2);
    const third = throttled(3);
    const secondRejection = assert.rejects(
      second,
      (error) => error.code === 'ABORT_ERR' && error.cause === reason,
    );
    const thirdRejection = assert.rejects(
      third,
      (error) => error.code === 'ABORT_ERR' && error.cause === reason,
    );

    throttled.cancel(reason);
    assert.strictEqual(throttled.pending, null);
    assert.strictEqual(throttled.pendingCount, 0);
    assert.strictEqual(await first, 1);
    await Promise.all([secondRejection, thirdRejection]);

    // Canceling resets the limiter, so the next call can run immediately.
    assert.strictEqual(await throttled(4), 4);
    throttled.cancel();
  }

  {
    const reason = new Error('stop');
    const controller = new AbortController();
    const throttled = throttle(common.mustCall((value) => value), 1, 100, {
      signal: controller.signal,
    });
    const first = throttled(1);
    const second = throttled(2);
    const secondRejection = assert.rejects(
      second,
      (error) => error.code === 'ABORT_ERR' && error.cause === reason,
    );

    controller.abort(reason);
    assert.strictEqual(throttled.hasImmediateCapacity(), false);
    assert.strictEqual(await first, 1);
    await secondRejection;
    assert.strictEqual(throttled.pending, null);
    assert.strictEqual(throttled.pendingCount, 0);
    await assert.rejects(
      throttled(3),
      (error) => error.code === 'ABORT_ERR' && error.cause === reason,
    );
  }

  {
    const expected = new Error('failure');
    const throttled = throttle(common.mustCall((value) => {
      if (value === 2) throw expected;
      return value;
    }, 2), 1, 20);

    assert.strictEqual(await throttled(1), 1);
    await assert.rejects(throttled(2), (error) => error === expected);
  }

  {
    const expected = new Error('async failure');
    const throttled = throttle(common.mustCall(async () => {
      throw expected;
    }), 1, 0);
    await assert.rejects(throttled(), (error) => error === expected);
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
    assert.deepStrictEqual(await Promise.all([first, recursive]), [1, 2]);
    assert.deepStrictEqual(values, [1, 2]);
  }

  {
    function original(first, second) {
      return first + second;
    }
    const throttled = throttle(original, 1, 0);
    assert.strictEqual(throttled.name, original.name);
    assert.strictEqual(throttled.length, original.length);
    assert.strictEqual(await throttled(1, 2), 3);
  }

  {
    const values = [];
    const throttled = throttle(common.mustCall((value) => {
      values.push(value);
      return value;
    }, 5), 1, 0);
    const calls = [];
    for (let i = 0; i < 5; i++) calls.push(throttled(i));
    assert.deepStrictEqual(values, [0, 1, 2, 3, 4]);
    assert.deepStrictEqual(await Promise.all(calls), [0, 1, 2, 3, 4]);
  }

  {
    let timeoutCount = 0;
    const hook = createHook({
      init(_asyncId, type) {
        if (type === 'Timeout') timeoutCount++;
      },
    });
    const throttled = throttle(common.mustCall((value) => value), 1, 100);

    hook.enable();
    assert.strictEqual(throttled.hasImmediateCapacity(), true);
    const call = throttled(1);
    assert.strictEqual(throttled.hasImmediateCapacity(), false);
    if (throttled.hasImmediateCapacity()) throttled(2);
    hook.disable();

    assert.strictEqual(timeoutCount, 0);
    assert.strictEqual(throttled.pendingCount, 0);
    assert.strictEqual(await call, 1);
  }

  {
    let timeoutCount = 0;
    const hook = createHook({
      init(_asyncId, type) {
        if (type === 'Timeout') timeoutCount++;
      },
    });
    const throttled = throttle(common.mustCall((value) => value), 1, 100);
    hook.enable();
    const first = throttled(1);
    const second = throttled(2);
    const third = throttled(3);
    hook.disable();
    const secondRejection = assert.rejects(second, { code: 'ABORT_ERR' });
    const thirdRejection = assert.rejects(third, { code: 'ABORT_ERR' });

    assert.strictEqual(timeoutCount, 1);
    throttled.cancel();
    assert.strictEqual(await first, 1);
    await Promise.all([secondRejection, thirdRejection]);
  }
})().then(common.mustCall());
