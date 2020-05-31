'use strict';
// Flags: --expose-internals

const common = require('../common');
const { once, EventEmitter } = require('events');
const { strictEqual, deepStrictEqual, fail } = require('assert');
const { EventTarget, Event } = require('internal/event_target');

async function onceAnEvent() {
  const ee = new EventEmitter();

  process.nextTick(() => {
    ee.emit('myevent', 42);
  });

  const [value] = await once(ee, 'myevent');
  strictEqual(value, 42);
  strictEqual(ee.listenerCount('error'), 0);
  strictEqual(ee.listenerCount('myevent'), 0);
}

async function onceAnEventWithTwoArgs() {
  const ee = new EventEmitter();

  process.nextTick(() => {
    ee.emit('myevent', 42, 24);
  });

  const value = await once(ee, 'myevent');
  deepStrictEqual(value, [42, 24]);
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
  strictEqual(err, expected);
  strictEqual(ee.listenerCount('error'), 0);
  strictEqual(ee.listenerCount('myevent'), 0);
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
  process.removeAllListeners('multipleResolves');
  strictEqual(err, expected);
  strictEqual(ee.listenerCount('error'), 0);
  strictEqual(ee.listenerCount('myevent'), 0);
}

async function onceError() {
  const ee = new EventEmitter();

  const expected = new Error('kaboom');
  process.nextTick(() => {
    ee.emit('error', expected);
  });

  const promise = once(ee, 'error');
  strictEqual(ee.listenerCount('error'), 1);
  const [ err ] = await promise;
  strictEqual(err, expected);
  strictEqual(ee.listenerCount('error'), 0);
  strictEqual(ee.listenerCount('myevent'), 0);
}

async function onceWithEventTarget() {
  const et = new EventTarget();
  const event = new Event('myevent');
  process.nextTick(() => {
    et.dispatchEvent(event);
  });
  const [ value ] = await once(et, 'myevent');
  strictEqual(value, event);
}

async function onceWithEventTargetError() {
  const et = new EventTarget();
  const error = new Event('error');
  process.nextTick(() => {
    et.dispatchEvent(error);
  });

  const [ err ] = await once(et, 'error');
  strictEqual(err, error);
}

async function prioritizesEventEmitter() {
  const ee = new EventEmitter();
  ee.addEventListener = fail;
  ee.removeAllListeners = fail;
  process.nextTick(() => ee.emit('foo'));
  await once(ee, 'foo');
}
Promise.all([
  onceAnEvent(),
  onceAnEventWithTwoArgs(),
  catchesErrors(),
  stopListeningAfterCatchingError(),
  onceError(),
  onceWithEventTarget(),
  onceWithEventTargetError(),
  prioritizesEventEmitter(),
]).then(common.mustCall());
