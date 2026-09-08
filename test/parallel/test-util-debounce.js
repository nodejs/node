// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('node:assert');
const { createHook } = require('node:async_hooks');
const { setTimeout } = require('node:timers/promises');
const { debounce } = require('node:util');
const { TIMEOUT_MAX } = require('internal/timers');

for (const value of [undefined, null, true, 0, 'fn', {}, [], Symbol()]) {
  assert.throws(() => debounce(value, 1), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
}

for (const value of [undefined, null, true, '1', {}, [], Symbol()]) {
  assert.throws(() => debounce(() => {}, value), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
}

for (const value of [-1, 0.5, NaN, Infinity, TIMEOUT_MAX + 1]) {
  assert.throws(() => debounce(() => {}, value), {
    code: 'ERR_OUT_OF_RANGE',
  });
}

for (const value of [null, true, 1, 'options', []]) {
  assert.throws(() => debounce(() => {}, 1, value), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
}

assert.throws(() => debounce(() => {}, 1, { rejectOnCancel: 1 }), {
  code: 'ERR_INVALID_ARG_TYPE',
});
assert.throws(() => debounce(() => {}, 1, { leading: 1 }), {
  code: 'ERR_INVALID_ARG_TYPE',
});
assert.throws(() => debounce(() => {}, 1, { signal: {} }), {
  code: 'ERR_INVALID_ARG_TYPE',
});

{
  const reason = new Error('already aborted');
  assert.throws(
    () => debounce(() => {}, 1, { signal: AbortSignal.abort(reason) }),
    (error) => error.code === 'ABORT_ERR' && error.cause === reason,
  );
}

(async () => {
  {
    const debounced = debounce(common.mustCall(function(...args) {
      assert.strictEqual(this, debounced);
      assert.strictEqual(debounced.pending, null);
      assert.deepStrictEqual(args, ['last', 2]);
      return args[1];
    }), 100);

    assert.strictEqual(typeof debounced.cancel, 'function');
    assert.strictEqual(typeof debounced.flush, 'function');
    assert.strictEqual(typeof debounced.ref, 'function');
    assert.strictEqual(typeof debounced.unref, 'function');
    assert.strictEqual(debounced.unref(), debounced);
    assert.strictEqual(debounced.ref(), debounced);
    assert.strictEqual(debounced.pending, null);
    assert.strictEqual(debounced.pendingCount, 0);

    const first = debounced('first', 1);
    const second = debounced('last', 2);
    assert(first instanceof Promise);
    assert(second instanceof Promise);
    assert.notStrictEqual(first, second);
    assert.strictEqual(debounced.pending, second);
    assert.strictEqual(debounced.pendingCount, 2);
    assert.strictEqual(debounced.unref(), debounced);
    assert.strictEqual(debounced.ref(), debounced);

    debounced.flush();
    assert.strictEqual(debounced.pending, null);
    assert.strictEqual(debounced.pendingCount, 0);
    debounced.flush();
    assert.deepStrictEqual(await Promise.all([first, second]), [2, 2]);
  }

  {
    const values = [];
    const debounced = debounce(common.mustCall((value) => {
      values.push(value);
      return value;
    }, 3), 100, { leading: true });

    const first = debounced(1);
    assert.deepStrictEqual(values, [1]);
    assert.strictEqual(debounced.pending, null);
    assert.strictEqual(debounced.pendingCount, 0);

    // With no trailing call pending, flush does not end the debounce window.
    debounced.flush();
    const second = debounced(2);
    const third = debounced(3);
    assert.deepStrictEqual(values, [1]);
    assert.strictEqual(debounced.pending, third);
    assert.strictEqual(debounced.pendingCount, 2);

    debounced.flush();
    assert.deepStrictEqual(values, [1, 3]);
    assert.deepStrictEqual(
      await Promise.all([first, second, third]),
      [1, 3, 3],
    );

    const fourth = debounced(4);
    assert.deepStrictEqual(values, [1, 3, 4]);
    assert.strictEqual(await fourth, 4);
    debounced.cancel();
  }

  {
    const debounced = debounce(common.mustCall((value) => value), 1, {
      leading: true,
    });
    assert.strictEqual(await debounced(42), 42);
    assert.strictEqual(debounced.pending, null);
    await setTimeout(10);
  }

  {
    let recursive;
    const values = [];
    const debounced = debounce(common.mustCall((value) => {
      values.push(value);
      if (value === 1) recursive = debounced(2);
      return value;
    }, 2), 100);

    const first = debounced(1);
    debounced.flush();
    assert.deepStrictEqual(values, [1]);
    assert.strictEqual(debounced.pending, recursive);
    assert.strictEqual(await first, 1);

    debounced.flush();
    assert.deepStrictEqual(values, [1, 2]);
    assert.strictEqual(await recursive, 2);
  }

  {
    let recursive;
    const values = [];
    const debounced = debounce(common.mustCall((value) => {
      values.push(value);
      if (value === 1) recursive = debounced(2);
      return value;
    }, 2), 100, { leading: true });

    const first = debounced(1);
    assert.deepStrictEqual(values, [1]);
    assert.strictEqual(debounced.pending, recursive);
    assert.strictEqual(await first, 1);

    debounced.flush();
    assert.deepStrictEqual(values, [1, 2]);
    assert.strictEqual(await recursive, 2);
  }

  {
    let recursive;
    const values = [];
    const debounced = debounce(common.mustCall((value) => {
      values.push(value);
      if (value === 2) recursive = debounced(3);
      return value;
    }, 3), 100, { leading: true });

    const first = debounced(1);
    const second = debounced(2);
    debounced.flush();
    assert.deepStrictEqual(values, [1, 2, 3]);
    assert.deepStrictEqual(
      await Promise.all([first, second, recursive]),
      [1, 2, 3],
    );
    assert.strictEqual(debounced.pending, null);
    debounced.cancel();
  }

  {
    const debounced = debounce(common.mustCall(async (value) => {
      await Promise.resolve();
      return value;
    }), 100);
    const result = debounced(42);
    debounced.flush();
    assert.strictEqual(await result, 42);
  }

  {
    const expected = new Error('failure');
    const debounced = debounce(common.mustCall(() => { throw expected; }), 100);
    const result = debounced();
    debounced.flush();
    await assert.rejects(result, (error) => error === expected);
  }

  {
    const debounced = debounce(common.mustNotCall(), 100);
    const first = debounced();
    const second = debounced();
    const reason = new Error('cancelled');
    debounced.cancel(reason);
    assert.strictEqual(debounced.pending, null);
    debounced.cancel();
    await assert.rejects(
      first,
      (error) => error.code === 'ABORT_ERR' && error.cause === reason,
    );
    await assert.rejects(
      second,
      (error) => error.code === 'ABORT_ERR' && error.cause === reason,
    );
  }

  {
    const debounced = debounce(common.mustCall((value) => value), 100, {
      rejectOnCancel: true,
    });
    const first = debounced(1);
    const firstRejection = assert.rejects(first, {
      code: 'ABORT_ERR',
      name: 'AbortError',
    });
    const second = debounced(2);
    assert.strictEqual(debounced.pendingCount, 1);
    await firstRejection;
    debounced.flush();
    assert.strictEqual(await second, 2);
  }

  {
    const reason = new Error('stop');
    const controller = new AbortController();
    const debounced = debounce(common.mustNotCall(), 100, {
      signal: controller.signal,
    });
    const result = debounced();
    controller.abort(reason);
    await assert.rejects(
      result,
      (error) => error.code === 'ABORT_ERR' && error.cause === reason,
    );
    assert.strictEqual(debounced.pending, null);
  }

  {
    const reason = new Error('stop future calls');
    const controller = new AbortController();
    const debounced = debounce(common.mustNotCall(), 100, {
      signal: controller.signal,
    });
    controller.abort(reason);
    await assert.rejects(
      debounced(),
      (error) => error.code === 'ABORT_ERR' && error.cause === reason,
    );
    await assert.rejects(
      debounced(),
      (error) => error.code === 'ABORT_ERR' && error.cause === reason,
    );
    assert.strictEqual(debounced.pending, null);
    assert.strictEqual(debounced.pendingCount, 0);
  }

  {
    const reason = new Error('abort during leading call');
    const controller = new AbortController();
    const state = {};
    controller.signal.addEventListener('abort', common.mustCall(() => {
      state.result = state.debounced();
    }));
    state.debounced = debounce(common.mustNotCall(), 100, {
      leading: true,
      signal: controller.signal,
    });
    controller.abort(reason);
    await assert.rejects(
      state.result,
      (error) => error.code === 'ABORT_ERR' && error.cause === reason,
    );
  }

  {
    const reason = new Error('abort during flush');
    const controller = new AbortController();
    const state = {};
    controller.signal.addEventListener('abort', common.mustCall(() => {
      state.debounced.flush();
    }));
    state.debounced = debounce(common.mustNotCall(), 100, {
      signal: controller.signal,
    });
    const result = state.debounced();
    controller.abort(reason);
    await assert.rejects(
      result,
      (error) => error.code === 'ABORT_ERR' && error.cause === reason,
    );
  }

  {
    const controller = new AbortController();
    const debounced = debounce(common.mustCall(() => 1), 100, {
      signal: controller.signal,
    });
    const result = debounced();
    debounced.flush();
    controller.abort();
    assert.strictEqual(await result, 1);
  }

  {
    const debounced = debounce(common.mustCall((value) => value), 1);
    assert.strictEqual(await debounced(42), 42);
  }

  {
    const debounced = debounce(common.mustCall((value) => value, 2), 100);
    const first = debounced(1);
    debounced.flush();
    assert.strictEqual(await first, 1);
    const second = debounced(2);
    debounced.flush();
    assert.strictEqual(await second, 2);
  }

  {
    function original(first, second) {
      return first + second;
    }
    const debounced = debounce(original, 100);
    assert.strictEqual(debounced.name, original.name);
    assert.strictEqual(debounced.length, original.length);
  }

  {
    let timeoutCount = 0;
    const hook = createHook({
      init(_asyncId, type) {
        if (type === 'Timeout') timeoutCount++;
      },
    });
    const debounced = debounce(common.mustCall((value) => value), 100);
    hook.enable();
    const first = debounced(1);
    const second = debounced(2);
    const third = debounced(3);
    hook.disable();
    assert.strictEqual(timeoutCount, 1);
    debounced.flush();
    assert.deepStrictEqual(await Promise.all([first, second, third]), [3, 3, 3]);
  }

  {
    const debounced = debounce(common.mustNotCall(), 100);
    debounced();
    const pending = debounced();
    const { promise, resolve } = Promise.withResolvers();
    process.once('unhandledRejection', common.mustCall((error, unhandled) => {
      assert.strictEqual(error.code, 'ABORT_ERR');
      assert.strictEqual(unhandled, pending);
      resolve();
    }));
    debounced.cancel();
    await promise;
  }

  // Give canceled timers time to expose any accidental extra invocation.
  await setTimeout(110);
})().then(common.mustCall());
