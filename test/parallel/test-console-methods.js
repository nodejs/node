'use strict';
require('../common');

// This test ensures that console methods cannot be invoked as constructors and
// that their name is always correct.

const assert = require('assert');

const { Console } = console;
const newInstance = new Console(process.stdout);
const err = TypeError;

const methods = [
  'log',
  'warn',
  'dir',
  'time',
  'timeEnd',
  'timeLog',
  'trace',
  'assert',
  'clear',
  'count',
  'countReset',
  'group',
  'groupEnd',
  'table',
  'debug',
  'info',
  'dirxml',
  'error',
  'groupCollapsed',
];

const alternateNames = {
  debug: 'log',
  info: 'log',
  dirxml: 'log',
  error: 'warn',
  groupCollapsed: 'group'
};

function assertEqualName(method) {
  try {
    assert.strictEqual(console[method].name, method);
  } catch {
    assert.strictEqual(console[method].name, alternateNames[method]);
  }
  try {
    assert.strictEqual(newInstance[method].name, method);
  } catch {
    assert.strictEqual(newInstance[method].name, alternateNames[method]);
  }
}

for (const method of methods) {
  assertEqualName(method);

  assert.throws(() => new console[method](), err);
  assert.throws(() => new newInstance[method](), err);
  assert.throws(() => Reflect.construct({}, [], console[method]), err);
  assert.throws(() => Reflect.construct({}, [], newInstance[method]), err);
}
