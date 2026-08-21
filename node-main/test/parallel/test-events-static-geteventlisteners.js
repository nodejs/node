'use strict';
// Flags: --expose-internals --no-warnings
const common = require('../common');
const { kWeakHandler } = require('internal/event_target');

const {
  deepStrictEqual,
  throws,
} = require('assert');

const { getEventListeners, EventEmitter } = require('events');

// Test getEventListeners on EventEmitter
{
  const fn1 = common.mustNotCall();
  const fn2 = common.mustNotCall();
  const emitter = new EventEmitter();
  emitter.on('foo', fn1);
  emitter.on('foo', fn2);
  emitter.on('baz', fn1);
  emitter.on('baz', fn1);
  deepStrictEqual(getEventListeners(emitter, 'foo'), [fn1, fn2]);
  deepStrictEqual(getEventListeners(emitter, 'bar'), []);
  deepStrictEqual(getEventListeners(emitter, 'baz'), [fn1, fn1]);
}
// Test getEventListeners on EventTarget
{
  const fn1 = common.mustNotCall();
  const fn2 = common.mustNotCall();
  const target = new EventTarget();
  target.addEventListener('foo', fn1);
  target.addEventListener('foo', fn2);
  target.addEventListener('baz', fn1);
  target.addEventListener('baz', fn1);
  deepStrictEqual(getEventListeners(target, 'foo'), [fn1, fn2]);
  deepStrictEqual(getEventListeners(target, 'bar'), []);
  deepStrictEqual(getEventListeners(target, 'baz'), [fn1]);
}

{
  throws(() => {
    getEventListeners('INVALID_EMITTER');
  }, /ERR_INVALID_ARG_TYPE/);
}
{
  // Test weak listeners
  const target = new EventTarget();
  const fn = common.mustNotCall();
  target.addEventListener('foo', fn, { [kWeakHandler]: {} });
  const listeners = getEventListeners(target, 'foo');
  deepStrictEqual(listeners, [fn]);
}
