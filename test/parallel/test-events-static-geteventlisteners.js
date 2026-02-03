'use strict';
// Flags: --expose-internals --no-warnings
const common = require('../common');
const { kWeakHandler } = require('internal/event_target');

const assert = require('assert');

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
  assert.deepStrictEqual(getEventListeners(emitter, 'foo'), [fn1, fn2]);
  assert.deepStrictEqual(getEventListeners(emitter, 'bar'), []);
  assert.deepStrictEqual(getEventListeners(emitter, 'baz'), [fn1, fn1]);
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
  assert.deepStrictEqual(getEventListeners(target, 'foo'), [fn1, fn2]);
  assert.deepStrictEqual(getEventListeners(target, 'bar'), []);
  assert.deepStrictEqual(getEventListeners(target, 'baz'), [fn1]);
}

{
  assert.throws(() => {
    getEventListeners('INVALID_EMITTER');
  }, /ERR_INVALID_ARG_TYPE/);
}
{
  // Test weak listeners
  const target = new EventTarget();
  const fn = common.mustNotCall();
  target.addEventListener('foo', fn, { [kWeakHandler]: {} });
  const listeners = getEventListeners(target, 'foo');
  assert.deepStrictEqual(listeners, [fn]);
}
