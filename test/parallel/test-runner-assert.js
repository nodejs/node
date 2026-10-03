'use strict';
require('../common');
const assert = require('node:assert');
const test = require('node:test');

test('expected methods are on t.assert', (t) => {
  const uncopiedKeys = [
    'AssertionError',
    'strict',
    'Assert',
    'options',
  ];
  const assertKeys = Object.keys(assert).filter((key) => !uncopiedKeys.includes(key));
  const expectedKeys = [
    'snapshot',
    'fileSnapshot',
    'called',
    'callCount',
    'calledWith',
  ].concat(assertKeys).sort();
  assert.deepStrictEqual(Object.keys(t.assert).sort(), expectedKeys);
});

test('t.assert.ok correctly parses the stacktrace', (t) => {
  t.assert.throws(() => t.assert.ok(1 === 2), /t\.assert\.ok\(1 === 2\)/);
});

test('t.assert.called', (t) => {
  const fn = t.mock.fn();
  t.assert.throws(() => t.assert.called(fn), {
    code: 'ERR_ASSERTION',
    message: 'Expected mock function to have been called',
  });
  fn();
  t.assert.called(fn);
  t.assert.throws(() => t.assert.called(() => {}), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

test('t.assert.callCount', (t) => {
  const fn = t.mock.fn();
  t.assert.callCount(fn, 0);
  fn();
  fn();
  t.assert.callCount(fn, 2);
  t.assert.throws(() => t.assert.callCount(fn, 1), {
    code: 'ERR_ASSERTION',
    message: 'Expected mock function to have been called 1 times, but it was called 2 times',
  });
  t.assert.throws(() => t.assert.callCount(fn, -1), {
    code: 'ERR_OUT_OF_RANGE',
  });
});

test('t.assert.calledWith', (t) => {
  const obj = { method() {} };
  t.mock.method(obj, 'method');
  obj.method(1, { a: 1 });
  obj.method('x');
  t.assert.calledWith(obj.method, 1, { a: 1 });
  t.assert.calledWith(obj.method, 'x');
  t.assert.throws(() => t.assert.calledWith(obj.method, 1), {
    code: 'ERR_ASSERTION',
    message: /called with \[ 1 \]/,
  });
  t.assert.throws(() => t.assert.calledWith(obj.method, '1', { a: 1 }), {
    code: 'ERR_ASSERTION',
  });
});

test('mock matchers count towards t.plan()', (t) => {
  t.plan(3);
  const fn = t.mock.fn();
  fn(1);
  t.assert.called(fn);
  t.assert.callCount(fn, 1);
  t.assert.calledWith(fn, 1);
});
