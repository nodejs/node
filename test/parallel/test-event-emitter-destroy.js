'use strict';

const common = require('../common');
const assert = require('assert');
const EventEmitter = require('events');

// If the optional callback parameter is not a function, throw
{
  const ee = new EventEmitter();
  common.expectsError(
    () => ee.destroy('something', 'else'), {
      code: 'ERR_INVALID_CALLBACK',
      type: TypeError
    }
  );
}

// If no error argument is passed to destroy, error will not be emitted,
// close will be emitted, and ee.destroyed will be true.
{
  const ee = new EventEmitter();
  assert.strictEqual(ee.destroyed, false);
  ee.destroy();
  ee.on('error', common.mustNotCall());
  ee.on('close', common.mustCall(() => {
    assert.strictEqual(ee.destroyed, true);
  }));
  ee.destroy();
}

// If destroy is called twice, once without an error, and once with an
// error, a close event will be emitted first, followed by an error event.
// Both will only be called once tho.
{
  let closeEmitted = false;
  const ee = new EventEmitter();
  assert.strictEqual(ee.destroyed, false);
  ee.destroy();
  ee.destroy(new Error('foo'));
  ee.on('error', common.expectsError({
    code: undefined,
    type: Error,
    message: 'foo'
  }));
  ee.on('error', common.mustCall(() => {
    assert.strictEqual(closeEmitted, true);
  }));
  ee.on('close', common.mustCall(() => {
    assert.strictEqual(ee.destroyed, true);
    closeEmitted = true;
  }));
}

// If destroy is called twice, once with an error, and once without, an
// error event will be emitted first, followed by a close event. Both will
// only be called once tho.
{
  let errorEmitted = false;
  const ee = new EventEmitter();
  assert.strictEqual(ee.destroyed, false);
  ee.destroy(new Error('foo'));
  ee.destroy();
  ee.on('error', common.expectsError({
    code: undefined,
    type: Error,
    message: 'foo'
  }));
  ee.on('error', common.mustCall(() => {
    errorEmitted = true;
  }));
  ee.on('close', common.mustCall(() => {
    assert.strictEqual(ee.destroyed, true);
    assert.strictEqual(errorEmitted, true);
  }));
}

// If a callback is provided, it will be invoked and an error event will not
// be emitted.
{
  let errorCallback = false;
  const ee = new EventEmitter();
  assert.strictEqual(ee.destroyed, false);
  ee.destroy(undefined, common.mustCall((err) => {
    assert.strictEqual(err, undefined);
    errorCallback = true;
  }));
  ee.on('error', common.mustNotCall());
  ee.on('close', common.mustCall(() => {
    assert.strictEqual(errorCallback, true);
  }));
}

// If destroy is called with an error and a callback is provided, the error
// will be sent to the callback and the error event will not be emitted.
{
  const ee = new EventEmitter();
  assert.strictEqual(ee.destroyed, false);
  ee.destroy(new Error('foo'), common.expectsError({
    code: undefined,
    type: Error,
    message: 'foo'
  }));
  ee.on('error', common.mustNotCall());
  ee.on('close', common.mustCall(() => {
    assert.strictEqual(ee.destroyed, true);
  }));
}

{
  class MyEmitter extends EventEmitter {
    constructor() {
      super();
      this._isDestroyed = false;
    }
    [EventEmitter.customDestroySymbol](err) {
      assert(err);
      assert.strictEqual(this._isDestroyed, false);
      assert.strictEqual(err.message, 'foo');
      this._isDestroyed = true;
    }
    [EventEmitter.customDestroyedSymbol]() {
      return this._isDestroyed;
    }
  }
  const ee = new MyEmitter();
  assert.strictEqual(ee.destroyed, false);
  assert.strictEqual(ee._isDestroyed, false);
  ee.destroy(new Error('foo'));
  assert.strictEqual(ee.destroyed, true);
  assert.strictEqual(ee._isDestroyed, true);
  ee.on('error', common.mustCall());
  ee.on('close', common.mustCall());
}
