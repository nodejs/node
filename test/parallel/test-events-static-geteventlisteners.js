// Flags: --expose-internals
'use strict';

const common = require('../common');

const {
  deepStrictEqual,
} = require('assert');

const { getEventListeners, EventEmitter } = require('events');
const { EventTarget } = require('internal/event_target');

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
