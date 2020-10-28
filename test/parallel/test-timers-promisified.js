// Flags: --no-warnings --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const timers = require('timers');
const { promisify } = require('util');
const child_process = require('child_process');

// TODO(benjamingr) - refactor to use getEventListeners when #35991 lands
const { NodeEventTarget } = require('internal/event_target');

const timerPromises = require('timers/promises');

/* eslint-disable no-restricted-syntax */

const setTimeout = promisify(timers.setTimeout);
const setImmediate = promisify(timers.setImmediate);
const setInterval = promisify(timers.setInterval);
const exec = promisify(child_process.exec);

assert.strictEqual(setTimeout, timerPromises.setTimeout);
assert.strictEqual(setImmediate, timerPromises.setImmediate);
assert.strictEqual(setInterval, timerPromises.setInterval);

process.on('multipleResolves', common.mustNotCall());

{
  const promise = setTimeout(1);
  promise.then(common.mustCall((value) => {
    assert.strictEqual(value, undefined);
  }));
}

{
  const promise = setTimeout(1, 'foobar');
  promise.then(common.mustCall((value) => {
    assert.strictEqual(value, 'foobar');
  }));
}

{
  const promise = setImmediate();
  promise.then(common.mustCall((value) => {
    assert.strictEqual(value, undefined);
  }));
}

{
  const promise = setImmediate('foobar');
  promise.then(common.mustCall((value) => {
    assert.strictEqual(value, 'foobar');
  }));
}

{
  const iterable = setInterval(1, 'foobar');
  const iterator = iterable[Symbol.asyncIterator]();
  const promise = iterator.next();
  promise
    .then(common.mustCall((result) => {
      assert.ok(!result.done);
      assert.strictEqual(result.value, 'foobar');
      return iterator.return();
    }))
    .then(common.mustCall());
}

{
  const iterable = setInterval(1);
  const iterator = iterable[Symbol.asyncIterator]();
  const promise = iterator.next();
  promise
    .then(common.mustCall((result) => {
      assert.ok(!result.done);
      assert.strictEqual(result.value, undefined);
      return iterator.return();
    }))
    .then(common.mustCall());
}

{
  const iterable = setInterval(1, 'foobar');
  const iterator = iterable[Symbol.asyncIterator]();
  const promise = iterator.next();
  promise
    .then(common.mustCall((result) => {
      assert.ok(!result.done);
      assert.strictEqual(result.value, 'foobar');
      return iterator.next();
    }))
    .then(common.mustCall((result) => {
      assert.ok(!result.done);
      assert.strictEqual(result.value, 'foobar');
      return iterator.return();
    }))
    .then(common.mustCall());
}

{
  const ac = new AbortController();
  const signal = ac.signal;
  assert.rejects(setTimeout(10, undefined, { signal }), /AbortError/);
  ac.abort();
}

{
  const ac = new AbortController();
  const signal = ac.signal;
  ac.abort(); // Abort in advance
  assert.rejects(setTimeout(10, undefined, { signal }), /AbortError/);
}

{
  const ac = new AbortController();
  const signal = ac.signal;
  assert.rejects(setImmediate(10, { signal }), /AbortError/);
  ac.abort();
}

{
  const ac = new AbortController();
  const signal = ac.signal;
  ac.abort(); // Abort in advance
  assert.rejects(setImmediate(10, { signal }), /AbortError/);
}

{
  const ac = new AbortController();
  const signal = ac.signal;
  ac.abort(); // Abort in advance

  const iterable = setInterval(1, undefined, { signal });
  const iterator = iterable[Symbol.asyncIterator]();

  assert.rejects(iterator.next(), /AbortError/);
}

{
  const ac = new AbortController();
  const signal = ac.signal;

  const iterable = setInterval(100, undefined, { signal });
  const iterator = iterable[Symbol.asyncIterator]();

  // This promise should take 100 seconds to resolve, so now aborting it should
  // mean we abort early
  const promise = iterator.next();

  ac.abort(); // Abort in after we have a next promise

  assert.rejects(promise, /AbortError/);
}

{
  // Check that aborting after resolve will not reject.
  const ac = new AbortController();
  const signal = ac.signal;
  setTimeout(10, undefined, { signal }).then(() => {
    ac.abort();
  });
}
{
  // Check that aborting after resolve will not reject.
  const ac = new AbortController();
  const signal = ac.signal;
  setImmediate(10, { signal }).then(() => {
    ac.abort();
  });
}

{
  // Check that timer adding signals does not leak handlers
  const signal = new NodeEventTarget();
  signal.aborted = false;
  setTimeout(0, null, { signal }).finally(common.mustCall(() => {
    assert.strictEqual(signal.listenerCount('abort'), 0);
  }));
}

{
  // Check that timer adding signals does not leak handlers
  const signal = new NodeEventTarget();
  signal.aborted = false;
  setImmediate(0, { signal }).finally(common.mustCall(() => {
    assert.strictEqual(signal.listenerCount('abort'), 0);
  }));
}

{
  Promise.all(
    [1, '', false, Infinity].map((i) => assert.rejects(setImmediate(10, i)), {
      code: 'ERR_INVALID_ARG_TYPE'
    })).then(common.mustCall());

  Promise.all(
    [1, '', false, Infinity, null, {}].map(
      (signal) => assert.rejects(setImmediate(10, { signal })), {
        code: 'ERR_INVALID_ARG_TYPE'
      })).then(common.mustCall());

  Promise.all(
    [1, '', Infinity, null, {}].map(
      (ref) => assert.rejects(setImmediate(10, { ref })), {
        code: 'ERR_INVALID_ARG_TYPE'
      })).then(common.mustCall());

  Promise.all(
    [1, '', false, Infinity].map(
      (i) => assert.rejects(setTimeout(10, null, i)), {
        code: 'ERR_INVALID_ARG_TYPE'
      })).then(common.mustCall());

  Promise.all(
    [1, '', false, Infinity, null, {}].map(
      (signal) => assert.rejects(setTimeout(10, null, { signal })), {
        code: 'ERR_INVALID_ARG_TYPE'
      })).then(common.mustCall());

  Promise.all(
    [1, '', Infinity, null, {}].map(
      (ref) => assert.rejects(setTimeout(10, null, { ref })), {
        code: 'ERR_INVALID_ARG_TYPE'
      })).then(common.mustCall());

  [1, '', Infinity, null, {}].forEach((ref) => {
    assert.throws(() => setInterval(10, undefined, { ref }));
  });
}

{
  exec(`${process.execPath} -pe "const assert = require('assert');` +
       'require(\'timers/promises\').setTimeout(1000, null, { ref: false }).' +
       'then(assert.fail)"').then(common.mustCall(({ stderr }) => {
    assert.strictEqual(stderr, '');
  }));
}

{
  exec(`${process.execPath} -pe "const assert = require('assert');` +
    'require(\'timers/promises\').setImmediate(null, { ref: false }).' +
    'then(assert.fail)"').then(common.mustCall(({ stderr }) => {
    assert.strictEqual(stderr, '');
  }));
}

{
  exec(`${process.execPath} -pe "const assert = require('assert');` +
    'const interval = require(\'timers/promises\')' +
    '.setInterval(1000, null, { ref: false });' +
    'interval[Symbol.asyncIterator]().next()' +
    '.then(assert.fail)"').then(common.mustCall(({ stderr }) => {
    assert.strictEqual(stderr, '');
  }));
}

{
  const ac = new AbortController();
  const input = 'foobar';
  const signal = ac.signal;

  const mainInterval = 5;
  const loopInterval = mainInterval * 1.5;

  const interval = setInterval(mainInterval, input, { signal });

  async function runInterval(fn) {
    const times = [];
    for await (const value of interval) {
      const index = times.length;
      times[index] = [Date.now()];
      assert.strictEqual(value, input);
      await fn();
      times[index] = [...times[index], Date.now()];
    }
  }

  const noopLoop = runInterval(() => {});
  const timeoutLoop = runInterval(() => setTimeout(loopInterval));

  // Let it loop 5 times, then abort before the next
  setTimeout(Math.floor(loopInterval * 5.5)).then(common.mustCall(() => {
    ac.abort();
  }));

  assert.rejects(noopLoop, /AbortError/);
  assert.rejects(timeoutLoop, /AbortError/);

}
