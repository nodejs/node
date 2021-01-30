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
const exec = promisify(child_process.exec);

assert.strictEqual(setTimeout, timerPromises.setTimeout);
assert.strictEqual(setImmediate, timerPromises.setImmediate);
const { setInterval } = timerPromises;

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
  const iterable = setInterval(1, undefined);
  const iterator = iterable[Symbol.asyncIterator]();
  const promise = iterator.next();
  promise.then(common.mustCall((result) => {
    assert.ok(!result.done, 'iterator was wrongly marked as done');
    assert.strictEqual(result.value, undefined);
    return iterator.return();
  })).then(common.mustCall());
}

{
  const iterable = setInterval(1, 'foobar');
  const iterator = iterable[Symbol.asyncIterator]();
  const promise = iterator.next();
  promise.then(common.mustCall((result) => {
    assert.ok(!result.done, 'iterator was wronly marked as done');
    assert.strictEqual(result.value, 'foobar');
    return iterator.return();
  })).then(common.mustCall());
}

{
  const iterable = setInterval(1, 'foobar');
  const iterator = iterable[Symbol.asyncIterator]();
  const promise = iterator.next();
  promise
    .then(common.mustCall((result) => {
      assert.ok(!result.done, 'iterator was wronly marked as done');
      assert.strictEqual(result.value, 'foobar');
      return iterator.next();
    }))
    .then(common.mustCall((result) => {
      assert.ok(!result.done, 'iterator was wrongly marked as done');
      assert.strictEqual(result.value, 'foobar');
      return iterator.return();
    }))
    .then(common.mustCall());
}

{
  const ac = new AbortController();
  const signal = ac.signal;
  assert.rejects(setTimeout(10, undefined, { signal }), /AbortError/)
    .then(common.mustCall());
  ac.abort();
}

{
  const ac = new AbortController();
  const signal = ac.signal;
  ac.abort(); // Abort in advance
  assert.rejects(setTimeout(10, undefined, { signal }), /AbortError/)
    .then(common.mustCall());
}

{
  const ac = new AbortController();
  const signal = ac.signal;
  assert.rejects(setImmediate(10, { signal }), /AbortError/)
    .then(common.mustCall());
  ac.abort();
}

{
  const ac = new AbortController();
  const signal = ac.signal;
  ac.abort(); // Abort in advance
  assert.rejects(setImmediate(10, { signal }), /AbortError/)
    .then(common.mustCall());
}

{
  const ac = new AbortController();
  const { signal } = ac;
  ac.abort(); // Abort in advance

  const iterable = setInterval(1, undefined, { signal });
  const iterator = iterable[Symbol.asyncIterator]();
  assert.rejects(iterator.next(), /AbortError/).then(common.mustCall());
}

{
  const ac = new AbortController();
  const { signal } = ac;

  const iterable = setInterval(100, undefined, { signal });
  const iterator = iterable[Symbol.asyncIterator]();

  // This promise should take 100 seconds to resolve, so now aborting it should
  // mean we abort early
  const promise = iterator.next();

  ac.abort(); // Abort in after we have a next promise

  assert.rejects(promise, /AbortError/).then(common.mustCall());
}

{
  // Check aborting after getting a value.
  const ac = new AbortController();
  const { signal } = ac;

  const iterable = setInterval(100, undefined, { signal });
  const iterator = iterable[Symbol.asyncIterator]();

  const promise = iterator.next();
  const abortPromise = promise.then(common.mustCall(() => ac.abort()))
    .then(() => iterator.next());
  assert.rejects(abortPromise, /AbortError/).then(common.mustCall());
}

{
  // Check that aborting after resolve will not reject.
  const ac = new AbortController();
  const signal = ac.signal;
  assert.doesNotReject(setTimeout(10, undefined, { signal })
    .then(common.mustCall(() => {
      ac.abort();
    }))).then(common.mustCall());
}
{
  // Check that aborting after resolve will not reject.
  const ac = new AbortController();
  const signal = ac.signal;
  assert.doesNotReject(setImmediate(10, { signal }).then(common.mustCall(() => {
    ac.abort();
  }))).then(common.mustCall());
}

{
  [1, '', Infinity, null, {}].forEach((ref) => {
    const iterable = setInterval(10, undefined, { ref });
    assert.rejects(() => iterable[Symbol.asyncIterator]().next(), /ERR_INVALID_ARG_TYPE/)
      .then(common.mustCall());
  });

  [1, '', Infinity, null, {}].forEach((signal) => {
    const iterable = setInterval(10, undefined, { signal });
    assert.rejects(() => iterable[Symbol.asyncIterator]().next(), /ERR_INVALID_ARG_TYPE/)
      .then(common.mustCall());
  });

  [1, '', Infinity, null, true, false].forEach((options) => {
    const iterable = setInterval(10, undefined, options);
    assert.rejects(() => iterable[Symbol.asyncIterator]().next(), /ERR_INVALID_ARG_TYPE/)
      .then(common.mustCall());
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
  // Check that timer adding signals does not leak handlers
  const signal = new NodeEventTarget();
  signal.aborted = false;
  const iterator = setInterval(1, undefined, { signal });
  iterator.next().then(common.mustCall(() => {
    assert.strictEqual(signal.listenerCount('abort'), 1);
    iterator.return();
  })).finally(common.mustCall(() => {
    assert.strictEqual(signal.listenerCount('abort'), 0);
  }));
}

{
  // Check that break removes the signal listener
  const signal = new NodeEventTarget();
  signal.aborted = false;
  async function tryBreak() {
    const iterator = setInterval(10, undefined, { signal });
    let i = 0;
    // eslint-disable-next-line no-unused-vars
    for await (const _ of iterator) {
      if (i === 0) {
        assert.strictEqual(signal.listenerCount('abort'), 1);
      }
      i++;
      if (i === 2) {
        break;
      }
    }
    assert.strictEqual(i, 2);
    assert.strictEqual(signal.listenerCount('abort'), 0);
  }

  tryBreak().then(common.mustCall());
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
  async function runInterval(fn, intervalTime, signal) {
    const input = 'foobar';
    const interval = setInterval(intervalTime, input, { signal });
    let iteration = 0;
    for await (const value of interval) {
      assert.strictEqual(value, input);
      iteration++;
      await fn(iteration);
    }
  }

  {
    // Check that we call the correct amount of times.
    const controller = new AbortController();
    const { signal } = controller;

    let loopCount = 0;
    const delay = 20;
    const timeoutLoop = runInterval(() => {
      loopCount++;
      if (loopCount === 5) controller.abort();
      if (loopCount > 5) throw new Error('ran too many times');
    }, delay, signal);

    assert.rejects(timeoutLoop, /AbortError/).then(common.mustCall(() => {
      assert.strictEqual(loopCount, 5);
    }));
  }

  {
    // Check that if we abort when we have some callbacks left,
    // we actually call them.
    const controller = new AbortController();
    const { signal } = controller;
    const delay = 10;
    let totalIterations = 0;
    const timeoutLoop = runInterval(async (iterationNumber) => {
      if (iterationNumber === 2) {
        await setTimeout(delay * 2);
        controller.abort();
      }
      if (iterationNumber > totalIterations) {
        totalIterations = iterationNumber;
      }
    }, delay, signal);

    timeoutLoop.catch(common.mustCall(() => {
      assert.ok(totalIterations >= 3, `iterations was ${totalIterations} < 3`);
    }));
  }
}

{
  // Check that the timing is correct
  let pre = false;
  let post = false;
  setTimeout(1).then(() => pre = true);
  const iterable = setInterval(2);
  const iterator = iterable[Symbol.asyncIterator]();

  iterator.next().then(common.mustCall(() => {
    assert.ok(pre, 'interval ran too early');
    assert.ok(!post, 'interval ran too late');
    return iterator.next();
  })).then(common.mustCall(() => {
    assert.ok(post, 'second interval ran too early');
    return iterator.return();
  }));

  setTimeout(3).then(() => post = true);
}
