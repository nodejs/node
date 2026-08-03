'use strict';

const common = require('../common');
const {
  hijackStderr,
  restoreStderr
} = require('../common/hijackstdio');
const assert = require('assert');

function test1() {
  // Output is skipped if the argument to the 'warning' event is
  // not an Error object.
  hijackStderr(common.mustNotCall('stderr.write must not be called'));
  process.emit('warning', 'test');
  setImmediate(test2);
}

function test2() {
  // Output is skipped if it's a deprecation warning and
  // process.noDeprecation = true
  process.noDeprecation = true;
  process.emitWarning('test', 'DeprecationWarning');
  process.noDeprecation = false;
  setImmediate(test3);
}

function test3() {
  restoreStderr();
  // Type defaults to warning when the second argument is an object
  process.emitWarning('test', {});
  process.once('warning', common.mustCall((warning) => {
    assert.strictEqual(warning.name, 'Warning');
  }));
  setImmediate(test4);
}

function test4() {
  // process.emitWarning will throw when process.throwDeprecation is true
  // and type is `DeprecationWarning`.
  process.throwDeprecation = true;
  process.once('uncaughtException', (err) => {
    assert.match(err.toString(), /^DeprecationWarning: test$/);
  });
  try {
    process.emitWarning('test', 'DeprecationWarning');
  } catch {
    assert.fail('Unreachable');
  }
  process.throwDeprecation = false;
  setImmediate(test5);
}

function test5() {
  // Setting toString to a non-function should not cause an error
  const err = new Error('test');
  err.toString = 1;
  process.emitWarning(err);
  setImmediate(test6);
}

function test6() {
  process.emitWarning('test', { detail: 'foo' });
  process.on('warning', (warning) => {
    assert.strictEqual(warning.detail, 'foo');
  });
}

test1();
