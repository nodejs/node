'use strict';
// Flags: --no-warnings

const common = require('../common');
const { once, EventEmitter, listenerCount } = require('events');
const assert = require('assert');

async function onceAnEvent() {
  const ee = new EventEmitter();

  process.nextTick(() => {
    ee.emit('myevent', 42);
  });

  const [value] = await once(ee, 'myevent');
  assert.strictEqual(value, 42);
  assert.strictEqual(ee.listenerCount('error'), 0);
  assert.strictEqual(ee.listenerCount('myevent'), 0);
}

async function onceAnEventWithInvalidOptions() {
  const ee = new EventEmitter();

  await Promise.all([1, 'hi', null, false, () => {}, Symbol(), 1n].map((options) => {
    return assert.rejects(once(ee, 'myevent', options), {
      code: 'ERR_INVALID_ARG_TYPE',
    });
  }));
}

async function onceAnEventWithTwoArgs() {
  const ee = new EventEmitter();

  process.nextTick(() => {
    ee.emit('myevent', 42, 24);
  });

  const value = await once(ee, 'myevent');
  assert.deepStrictEqual(value, [42, 24]);
}

async function catchesErrors() {
  const ee = new EventEmitter();

  const expected = new Error('kaboom');
  let err;
  process.nextTick(() => {
    ee.emit('error', expected);
  });

  try {
    await once(ee, 'myevent');
  } catch (_e) {
    err = _e;
  }
  assert.strictEqual(err, expected);
  assert.strictEqual(ee.listenerCount('error'), 0);
  assert.strictEqual(ee.listenerCount('myevent'), 0);
}

async function catchesErrorsWithAbortSignal() {
  const ee = new EventEmitter();
  const ac = new AbortController();
  const signal = ac.signal;

  const expected = new Error('boom');
  let err;
  process.nextTick(() => {
    ee.emit('error', expected);
  });

  try {
    const promise = once(ee, 'myevent', { signal });
    assert.strictEqual(ee.listenerCount('error'), 1);
    assert.strictEqual(listenerCount(signal, 'abort'), 1);

    await promise;
  } catch (e) {
    err = e;
  }
  assert.strictEqual(err, expected);
  assert.strictEqual(ee.listenerCount('error'), 0);
  assert.strictEqual(ee.listenerCount('myevent'), 0);
  assert.strictEqual(listenerCount(signal, 'abort'), 0);
}

async function stopListeningAfterCatchingError() {
  const ee = new EventEmitter();

  const expected = new Error('kaboom');
  let err;
  process.nextTick(() => {
    ee.emit('error', expected);
    ee.emit('myevent', 42, 24);
  });

  try {
    await once(ee, 'myevent');
  } catch (_e) {
    err = _e;
  }
  assert.strictEqual(err, expected);
  assert.strictEqual(ee.listenerCount('error'), 0);
  assert.strictEqual(ee.listenerCount('myevent'), 0);
}

async function onceError() {
  const ee = new EventEmitter();

  const expected = new Error('kaboom');
  process.nextTick(() => {
    ee.emit('error', expected);
  });

  const promise = once(ee, 'error');
  assert.strictEqual(ee.listenerCount('error'), 1);
  const [ err ] = await promise;
  assert.strictEqual(err, expected);
  assert.strictEqual(ee.listenerCount('error'), 0);
  assert.strictEqual(ee.listenerCount('myevent'), 0);
}

async function onceWithEventTarget() {
  const et = new EventTarget();
  const event = new Event('myevent');
  process.nextTick(() => {
    et.dispatchEvent(event);
  });
  const [ value ] = await once(et, 'myevent');
  assert.strictEqual(value, event);
}

async function onceWithEventTargetError() {
  const et = new EventTarget();
  const error = new Event('error');
  process.nextTick(() => {
    et.dispatchEvent(error);
  });

  const [ err ] = await once(et, 'error');
  assert.strictEqual(err, error);
}

async function onceWithInvalidEventEmmiter() {
  const ac = new AbortController();
  return assert.rejects(once(ac, 'myevent'), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
}

async function prioritizesEventEmitter() {
  const ee = new EventEmitter();
  ee.addEventListener = assert.fail;
  ee.removeAllListeners = assert.fail;
  process.nextTick(() => ee.emit('foo'));
  await once(ee, 'foo');
}

async function abortSignalBefore() {
  const ee = new EventEmitter();
  ee.on('error', common.mustNotCall());
  const abortedSignal = AbortSignal.abort();

  await Promise.all([1, {}, 'hi', null, false].map((signal) => {
    return assert.rejects(once(ee, 'foo', { signal }), {
      code: 'ERR_INVALID_ARG_TYPE',
    });
  }));

  return assert.rejects(once(ee, 'foo', { signal: abortedSignal }), {
    name: 'AbortError',
  });
}

async function abortSignalAfter() {
  const ee = new EventEmitter();
  const ac = new AbortController();
  ee.on('error', common.mustNotCall());
  const r = assert.rejects(once(ee, 'foo', { signal: ac.signal }), {
    name: 'AbortError',
  });
  process.nextTick(() => ac.abort());
  return r;
}

async function abortSignalAfterEvent() {
  const ee = new EventEmitter();
  const ac = new AbortController();
  process.nextTick(() => {
    ee.emit('foo');
    ac.abort();
  });
  const promise = once(ee, 'foo', { signal: ac.signal });
  assert.strictEqual(listenerCount(ac.signal, 'abort'), 1);
  await promise;
  assert.strictEqual(listenerCount(ac.signal, 'abort'), 0);
}

async function abortSignalRemoveListener() {
  const ee = new EventEmitter();
  const ac = new AbortController();

  try {
    process.nextTick(() => ac.abort());
    await once(ee, 'test', { signal: ac.signal });
  } catch {
    assert.strictEqual(ee.listeners('test').length, 0);
    assert.strictEqual(ee.listeners('error').length, 0);
  }
}

async function eventTargetAbortSignalBefore() {
  const et = new EventTarget();
  const abortedSignal = AbortSignal.abort();

  await Promise.all([1, {}, 'hi', null, false].map((signal) => {
    return assert.rejects(once(et, 'foo', { signal }), {
      code: 'ERR_INVALID_ARG_TYPE',
    });
  }));

  return assert.rejects(once(et, 'foo', { signal: abortedSignal }), {
    name: 'AbortError',
  });
}

async function eventTargetAbortSignalBeforeEvenWhenSignalPropagationStopped() {
  const et = new EventTarget();
  const ac = new AbortController();
  const { signal } = ac;
  signal.addEventListener('abort', (e) => e.stopImmediatePropagation(), { once: true });

  process.nextTick(() => ac.abort());
  return assert.rejects(once(et, 'foo', { signal }), {
    name: 'AbortError',
  });
}

async function eventTargetAbortSignalAfter() {
  const et = new EventTarget();
  const ac = new AbortController();
  const r = assert.rejects(once(et, 'foo', { signal: ac.signal }), {
    name: 'AbortError',
  });
  process.nextTick(() => ac.abort());
  return r;
}

async function eventTargetAbortSignalAfterEvent() {
  const et = new EventTarget();
  const ac = new AbortController();
  process.nextTick(() => {
    et.dispatchEvent(new Event('foo'));
    ac.abort();
  });
  await once(et, 'foo', { signal: ac.signal });
}

Promise.all([
  onceAnEvent(),
  onceAnEventWithInvalidOptions(),
  onceAnEventWithTwoArgs(),
  catchesErrors(),
  catchesErrorsWithAbortSignal(),
  stopListeningAfterCatchingError(),
  onceError(),
  onceWithEventTarget(),
  onceWithEventTargetError(),
  onceWithInvalidEventEmmiter(),
  prioritizesEventEmitter(),
  abortSignalBefore(),
  abortSignalAfter(),
  abortSignalAfterEvent(),
  abortSignalRemoveListener(),
  eventTargetAbortSignalBefore(),
  eventTargetAbortSignalBeforeEvenWhenSignalPropagationStopped(),
  eventTargetAbortSignalAfter(),
  eventTargetAbortSignalAfterEvent(),
]).then(common.mustCall());
