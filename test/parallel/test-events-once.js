'use strict';

const common = require('../common');
const { once, EventEmitter } = require('events');
const { strictEqual, deepStrictEqual } = require('assert');

class EventTargetMock {
  constructor() {
    this.events = {};
  }

  addEventListener = common.mustCall(function(name, listener, options) {
    if (!(name in this.events)) {
      this.events[name] = { listeners: [], options };
    }
    this.events[name].listeners.push(listener);
  });

  removeEventListener = common.mustCall(function(name, callback) {
    if (!(name in this.events)) {
      return;
    }
    const event = this.events[name];
    const stack = event.listeners;

    for (let i = 0, l = stack.length; i < l; i++) {
      if (stack[i] === callback) {
        stack.splice(i, 1);
        if (stack.length === 0) {
          Reflect.deleteProperty(this.events, name);
        }
        return;
      }
    }
  });

  dispatchEvent = function(name, ...arg) {
    if (!(name in this.events)) {
      return true;
    }
    const event = this.events[name];
    const stack = event.listeners.slice();

    for (let i = 0, l = stack.length; i < l; i++) {
      stack[i].apply(this, arg);
      if (event.options.once) {
        this.removeEventListener(name, stack[i]);
      }
    }
    return !name.defaultPrevented;
  };
}

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
  const et = new EventTargetMock();

  process.nextTick(() => {
    et.dispatchEvent('myevent', 42);
  });
  const [ value ] = await once(et, 'myevent');
  strictEqual(value, 42);
  strictEqual(Reflect.has(et.events, 'myevent'), false);
}

async function onceWithEventTargetTwoArgs() {
  const et = new EventTargetMock();

  process.nextTick(() => {
    et.dispatchEvent('myevent', 42, 24);
  });

  const value = await once(et, 'myevent');
  deepStrictEqual(value, [42, 24]);
}

async function onceWithEventTargetError() {
  const et = new EventTargetMock();

  const expected = new Error('kaboom');
  process.nextTick(() => {
    et.dispatchEvent('error', expected);
  });

  const [err] = await once(et, 'error');
  strictEqual(err, expected);
  strictEqual(Reflect.has(et.events, 'error'), false);
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
