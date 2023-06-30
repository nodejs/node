'use strict';
const common = require('../common');
const assert = require('assert');
const { EventEmitter } = require('events');
const { inherits } = require('util');

// Inherits from EE without a call to the
// parent constructor.
function NoConstructor() {
}

// captureExceptions param validation
{
  [1, [], function() {}, {}, Infinity, Math.PI, 'meow'].forEach((arg) => {
    assert.throws(
      () => new EventEmitter({ captureExceptions: arg }),
      {
        name: 'TypeError',
        code: 'ERR_INVALID_ARG_TYPE',
        message: 'The "options.captureExceptions" property must be of type boolean.' +
                 common.invalidArgTypeHelper(arg),
      }
    );
  });
}

inherits(NoConstructor, EventEmitter);

function captureExceptions() {
  const ee = new EventEmitter({ captureExceptions: true });
  const _err = new Error('kaboom');
  ee.on('something', common.mustCall((value) => {
    throw _err;
  }));

  ee.on('error', common.mustCall((err) => {
    assert.strictEqual(err, _err);
    process.nextTick(captureRejectionsTwoHandlers);
  }));

  ee.emit('something');
}

function captureRejectionsTwoHandlers() {
  const ee = new EventEmitter({ captureExceptions: true });
  const _err = new Error('kaboom');

  ee.on('something', common.mustCall((value) => {
    throw _err;
  }));

  // throw twice
  ee.on('something', common.mustCall((value) => {
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
  ee.on('something', common.mustCall((value) => {
    throw _err;
  }));

  process.removeAllListeners('uncaughtException');

  process.once('uncaughtException', common.mustCall((err) => {
    // restore default
    process.on('uncaughtException', (err) => { throw err; });

    assert.strictEqual(err, _err);
    process.nextTick(globalSetting);
  }));

  ee.emit('something');
}

function globalSetting() {
  assert.strictEqual(EventEmitter.captureExceptions, false);
  EventEmitter.captureExceptions = true;
  const ee = new EventEmitter();
  const _err = new Error('kaboom');
  ee.on('something', common.mustCall((value) => {
    throw _err;
  }));

  ee.on('error', common.mustCall((err) => {
    assert.strictEqual(err, _err);

    // restore default
    EventEmitter.captureExceptions = false;

    // TODO - run next test
    // process.nextTick();
  }));

  ee.emit('something');
}

// TODO - continue tests


captureExceptions();
