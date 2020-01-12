'use strict';
const common = require('../common');
const assert = require('assert');
const { EventEmitter, captureRejectionSymbol } = require('events');
const { inherits } = require('util');

// Inherits from EE without a call to the
// parent constructor.
function NoConstructor() {
}

inherits(NoConstructor, EventEmitter);

function captureRejections() {
  const ee = new EventEmitter({ captureRejections: true });
  const _err = new Error('kaboom');
  ee.on('something', common.mustCall(async (value) => {
    throw _err;
  }));

  ee.on('error', common.mustCall((err) => {
    assert.strictEqual(err, _err);
    process.nextTick(captureRejectionsTwoHandlers);
  }));

  ee.emit('something');
}

function captureRejectionsTwoHandlers() {
  const ee = new EventEmitter({ captureRejections: true });
  const _err = new Error('kaboom');

  ee.on('something', common.mustCall(async (value) => {
    throw _err;
  }));

  // throw twice
  ee.on('something', common.mustCall(async (value) => {
    throw _err;
  }));

  let count = 0;

  ee.on('error', common.mustCall((err) => {
    assert.strictEqual(err, _err);
    if (++count === 2) {
      process.nextTick(defaultValue);
    }
  }, 2));

  ee.emit('something');
}

function defaultValue() {
  const ee = new EventEmitter();
  const _err = new Error('kaboom');
  ee.on('something', common.mustCall(async (value) => {
    throw _err;
  }));

  process.removeAllListeners('unhandledRejection');

  process.once('unhandledRejection', common.mustCall((err) => {
    // restore default
    process.on('unhandledRejection', (err) => { throw err; });

    assert.strictEqual(err, _err);
    process.nextTick(globalSetting);
  }));

  ee.emit('something');
}

function globalSetting() {
  assert.strictEqual(EventEmitter.captureRejections, false);
  EventEmitter.captureRejections = true;
  const ee = new EventEmitter();
  const _err = new Error('kaboom');
  ee.on('something', common.mustCall(async (value) => {
    throw _err;
  }));

  ee.on('error', common.mustCall((err) => {
    assert.strictEqual(err, _err);

    // restore default
    EventEmitter.captureRejections = false;
    process.nextTick(configurable);
  }));

  ee.emit('something');
}

// We need to be able to configure this for streams, as we would
// like to call destro(err) there.
function configurable() {
  const ee = new EventEmitter({ captureRejections: true });
  const _err = new Error('kaboom');
  ee.on('something', common.mustCall(async (...args) => {
    assert.deepStrictEqual(args, [42, 'foobar']);
    throw _err;
  }));

  assert.strictEqual(captureRejectionSymbol, Symbol.for('nodejs.rejection'));

  ee[captureRejectionSymbol] = common.mustCall((err, type, ...args) => {
    assert.strictEqual(err, _err);
    assert.strictEqual(type, 'something');
    assert.deepStrictEqual(args, [42, 'foobar']);
    process.nextTick(globalSettingNoConstructor);
  });

  ee.emit('something', 42, 'foobar');
}

function globalSettingNoConstructor() {
  assert.strictEqual(EventEmitter.captureRejections, false);
  EventEmitter.captureRejections = true;
  const ee = new NoConstructor();
  const _err = new Error('kaboom');
  ee.on('something', common.mustCall(async (value) => {
    throw _err;
  }));

  ee.on('error', common.mustCall((err) => {
    assert.strictEqual(err, _err);

    // restore default
    EventEmitter.captureRejections = false;
    process.nextTick(thenable);
  }));

  ee.emit('something');
}

function thenable() {
  const ee = new EventEmitter({ captureRejections: true });
  const _err = new Error('kaboom');
  ee.on('something', common.mustCall((value) => {
    const obj = {};

    Object.defineProperty(obj, 'then', {
      get: common.mustCall(() => {
        return common.mustCall((resolved, rejected) => {
          assert.strictEqual(resolved, undefined);
          rejected(_err);
        });
      }, 1) // Only 1 call for Promises/A+ compat.
    });

    return obj;
  }));

  ee.on('error', common.mustCall((err) => {
    assert.strictEqual(err, _err);
    process.nextTick(avoidLoopOnRejection);
  }));

  ee.emit('something');
}

function avoidLoopOnRejection() {
  const ee = new EventEmitter({ captureRejections: true });
  const _err1 = new Error('kaboom');
  const _err2 = new Error('kaboom2');
  ee.on('something', common.mustCall(async (value) => {
    throw _err1;
  }));

  ee[captureRejectionSymbol] = common.mustCall(async (err) => {
    assert.strictEqual(err, _err1);
    throw _err2;
  });

  process.removeAllListeners('unhandledRejection');

  process.once('unhandledRejection', common.mustCall((err) => {
    // restore default
    process.on('unhandledRejection', (err) => { throw err; });

    assert.strictEqual(err, _err2);
    process.nextTick(avoidLoopOnError);
  }));

  ee.emit('something');
}

function avoidLoopOnError() {
  const ee = new EventEmitter({ captureRejections: true });
  const _err1 = new Error('kaboom');
  const _err2 = new Error('kaboom2');
  ee.on('something', common.mustCall(async (value) => {
    throw _err1;
  }));

  ee.on('error', common.mustCall(async (err) => {
    assert.strictEqual(err, _err1);
    throw _err2;
  }));

  process.removeAllListeners('unhandledRejection');

  process.once('unhandledRejection', common.mustCall((err) => {
    // restore default
    process.on('unhandledRejection', (err) => { throw err; });

    assert.strictEqual(err, _err2);
    process.nextTick(thenableThatThrows);
  }));

  ee.emit('something');
}

function thenableThatThrows() {
  const ee = new EventEmitter({ captureRejections: true });
  const _err = new Error('kaboom');
  ee.on('something', common.mustCall((value) => {
    const obj = {};

    Object.defineProperty(obj, 'then', {
      get: common.mustCall(() => {
        throw _err;
      }, 1) // Only 1 call for Promises/A+ compat.
    });

    return obj;
  }));

  ee.on('error', common.mustCall((err) => {
    assert.strictEqual(err, _err);
    process.nextTick(resetCaptureOnThrowInError);
  }));

  ee.emit('something');
}

function resetCaptureOnThrowInError() {
  const ee = new EventEmitter({ captureRejections: true });
  ee.on('something', common.mustCall(async (value) => {
    throw new Error('kaboom');
  }));

  ee.once('error', common.mustCall((err) => {
    throw err;
  }));

  process.removeAllListeners('uncaughtException');

  process.once('uncaughtException', common.mustCall((err) => {
    process.nextTick(next);
  }));

  ee.emit('something');

  function next() {
    process.on('uncaughtException', common.mustNotCall());

    const _err = new Error('kaboom2');
    ee.on('something2', common.mustCall(async (value) => {
      throw _err;
    }));

    ee.on('error', common.mustCall((err) => {
      assert.strictEqual(err, _err);

      process.removeAllListeners('uncaughtException');

      // restore default
      process.on('uncaughtException', (err) => { throw err; });

      process.nextTick(argValidation);
    }));

    ee.emit('something2');
  }
}

function argValidation() {

  function testType(obj) {
    assert.throws(() => new EventEmitter({ captureRejections: obj }), {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    });

    assert.throws(() => EventEmitter.captureRejections = obj, {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    });
  }

  testType([]);
  testType({ hello: 42 });
  testType(42);
}

captureRejections();
