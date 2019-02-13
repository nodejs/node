'use strict';

const common = require('../common');
const { once, EventEmitter } = require('events');
const { strictEqual, deepStrictEqual } = require('assert');

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

  process.on('multipleResolves', common.mustNotCall());

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

  const [err] = await once(ee, 'error');
  strictEqual(err, expected);
  strictEqual(ee.listenerCount('error'), 0);
  strictEqual(ee.listenerCount('myevent'), 0);
}

Promise.all([
  onceAnEvent(),
  onceAnEventWithTwoArgs(),
  catchesErrors(),
  stopListeningAfterCatchingError(),
  onceError()
]);
