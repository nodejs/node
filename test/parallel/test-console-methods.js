'use strict';
require('../common');

// This test ensures that console methods
// cannot be invoked as constructors

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

for (const method of methods) {
  assert.throws(() => new console[method](), err);
  assert.throws(() => new newInstance[method](), err);
  assert.throws(() => Reflect.construct({}, [], console[method]), err);
  assert.throws(() => Reflect.construct({}, [], newInstance[method]), err);
}
