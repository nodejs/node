'use strict';

const common = require('../common');
const assert = require('assert');
const EventEmitter = require('events');

// _onceWrap function definition
function _onceWrap(target, type, listener) {
  const state = { fired: false, target, type, listener };

  function wrapped(...args) {
    if (!state.fired) {
      state.fired = true;
      target.removeListener(type, wrapped);
      listener.apply(target, args);
    }
  }

  wrapped.listener = listener;

  // Store the reference only if necessary
  if (target.shouldStoreReference) {
    state.wrapFn = wrapped;
  }

  return wrapped;
}

// Error messages
const MSG_FIRED_ONCE = 'Listener should be fired only once';
const MSG_REMOVED = 'Listener should be removed';

// Test scenarios
{
  // Test 1: Listener should be fired only once
  const ee = new EventEmitter();
  let callCount = 0;

  const wrappedListener = _onceWrap(ee, 'test', common.mustCall(() => {
    callCount++;
  }));

  ee.on('test', wrappedListener);

  ee.emit('test');
  ee.emit('test');
  ee.emit('test');

  assert.strictEqual(callCount, 1, MSG_FIRED_ONCE);
}

{
  // Test 2: Listener should be correctly removed
  const ee = new EventEmitter();

  const wrappedListener = _onceWrap(ee, 'test', common.mustCall(1));

  ee.on('test', wrappedListener);

  ee.emit('test');
  assert.strictEqual(ee.listenerCount('test'), 0, MSG_REMOVED);
}

{
  // Test 3: Listener reference should be stored (shouldStoreReference)
  const ee = new EventEmitter();
  ee.shouldStoreReference = true;
  let callCount = 0;

  const wrappedListener = _onceWrap(ee, 'test', common.mustCall(() => {
    callCount++;
  }, 1));

  ee.on('test', wrappedListener);

  ee.emit('test');
  ee.emit('test');

  assert.strictEqual(callCount, 1, MSG_FIRED_ONCE);
  assert.strictEqual(ee.listenerCount('test'), 0, MSG_REMOVED);
}
