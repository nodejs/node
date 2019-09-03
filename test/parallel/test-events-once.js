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

async function onceWithEventTarget() {
  const emitter = new class EventTargetLike extends EventEmitter {
    addEventListener = common.mustCall(function(name, listener, options) {
      if (options.once) {
        this.once(name, listener);
      } else {
        this.on(name, listener);
      }
    });
  }();

  process.nextTick(() => {
    emitter.emit('myevent', 42);
  });
  const [ value ] = await once(emitter, 'myevent');
  strictEqual(value, 42);
  strictEqual(emitter.listenerCount('myevent'), 0);
}

async function onceWithEventTargetTwoArgs() {
  const emitter = new class EventTargetLike extends EventEmitter {
    addEventListener = common.mustCall(function(name, listener, options) {
      if (options.once) {
        this.once(name, listener);
      } else {
        this.on(name, listener);
      }
    });
  }();

  process.nextTick(() => {
    emitter.emit('myevent', 42, 24);
  });

  const value = await once(emitter, 'myevent');
  deepStrictEqual(value, [42, 24]);
}

async function onceWithEventTargetError() {
  const emitter = new EventEmitter();
  emitter.addEventListener = () => {};
  const expected = 'EventTarget does not have `error` event semantics';
  let actual;
  try {
    await once(emitter, 'error');
  } catch (error) {
    actual = error;
  }
  strictEqual(emitter.listenerCount('error'), 0);
  strictEqual(expected, actual);
}

Promise.all([
  onceAnEvent(),
  onceAnEventWithTwoArgs(),
  catchesErrors(),
  stopListeningAfterCatchingError(),
  onceError(),
  onceWithEventTarget(),
  onceWithEventTargetTwoArgs(),
  onceWithEventTargetError(),
]).then(common.mustCall());
