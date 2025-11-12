'use strict';
// Flags: --expose-internals
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const vm = require('vm');
const { promisify } = require('util');
const { customPromisifyArgs } = require('internal/util');

{
  const warningHandler = common.mustNotCall();
  process.on('warning', warningHandler);
  function foo() {}
  foo.constructor = (async () => {}).constructor;
  promisify(foo);
  process.off('warning', warningHandler);
}

common.expectWarning(
  'DeprecationWarning',
  'Calling promisify on a function that returns a Promise is likely a mistake.',
  'DEP0174');
promisify(async (callback) => { callback(); })().then(common.mustCall(() => {
  // We must add the second `expectWarning` call in the `.then` handler, when
  // the first warning has already been triggered.
  common.expectWarning(
    'DeprecationWarning',
    'Calling promisify on a function that returns a Promise is likely a mistake.',
    'DEP0174');
  promisify(async () => {})().then(common.mustNotCall());
}));

const stat = promisify(fs.stat);

{
  const promise = stat(__filename);
  assert(promise instanceof Promise);
  promise.then(common.mustCall((value) => {
    assert.deepStrictEqual(value, fs.statSync(__filename));
  }));
}

{
  const promise = stat('/dontexist');
  promise.catch(common.mustCall((error) => {
    assert(error.message.includes('ENOENT: no such file or directory, stat'));
  }));
}

{
  function fn() {}

  function promisifedFn() {}
  fn[promisify.custom] = promisifedFn;
  assert.strictEqual(promisify(fn), promisifedFn);
  assert.strictEqual(promisify(promisify(fn)), promisifedFn);
}

{
  function fn() {}

  function promisifiedFn() {}

  // util.promisify.custom is a shared symbol which can be accessed
  // as `Symbol.for("nodejs.util.promisify.custom")`.
  const kCustomPromisifiedSymbol = Symbol.for('nodejs.util.promisify.custom');
  fn[kCustomPromisifiedSymbol] = promisifiedFn;

  assert.strictEqual(kCustomPromisifiedSymbol, promisify.custom);
  assert.strictEqual(promisify(fn), promisifiedFn);
  assert.strictEqual(promisify(promisify(fn)), promisifiedFn);
}

{
  function fn() {}
  fn[promisify.custom] = 42;
  assert.throws(
    () => promisify(fn),
    { code: 'ERR_INVALID_ARG_TYPE', name: 'TypeError' }
  );
}

{
  const firstValue = 5;
  const secondValue = 17;

  function fn(callback) {
    callback(null, firstValue, secondValue);
  }

  fn[customPromisifyArgs] = ['first', 'second'];

  promisify(fn)().then(common.mustCall((obj) => {
    assert.deepStrictEqual(obj, { first: firstValue, second: secondValue });
  }));
}

{
  const fn = vm.runInNewContext('(function() {})');
  assert.notStrictEqual(Object.getPrototypeOf(promisify(fn)),
                        Function.prototype);
}

{
  function fn(callback) {
    callback(null, 'foo', 'bar');
  }
  promisify(fn)().then(common.mustCall((value) => {
    assert.strictEqual(value, 'foo');
  }));
}

{
  function fn(callback) {
    callback(null);
  }
  promisify(fn)().then(common.mustCall((value) => {
    assert.strictEqual(value, undefined);
  }));
}

{
  function fn(callback) {
    callback();
  }
  promisify(fn)().then(common.mustCall((value) => {
    assert.strictEqual(value, undefined);
  }));
}

{
  function fn(err, val, callback) {
    callback(err, val);
  }
  promisify(fn)(null, 42).then(common.mustCall((value) => {
    assert.strictEqual(value, 42);
  }));
}

{
  function fn(err, val, callback) {
    callback(err, val);
  }
  promisify(fn)(new Error('oops'), null).catch(common.mustCall((err) => {
    assert.strictEqual(err.message, 'oops');
  }));
}

{
  function fn(err, val, callback) {
    callback(err, val);
  }

  (async () => {
    const value = await promisify(fn)(null, 42);
    assert.strictEqual(value, 42);
  })().then(common.mustCall());
}

{
  const o = {};
  const fn = promisify(function(cb) {

    cb(null, this === o);
  });

  o.fn = fn;

  o.fn().then(common.mustCall((val) => assert(val)));
}

{
  const err = new Error('Should not have called the callback with the error.');
  const stack = err.stack;

  const fn = promisify(function(cb) {
    cb(null);
    cb(err);
  });

  (async () => {
    await fn();
    await Promise.resolve();
    return assert.strictEqual(stack, err.stack);
  })().then(common.mustCall());
}

{
  function c() { }
  const a = promisify(function() { });
  const b = promisify(a);
  assert.notStrictEqual(c, a);
  assert.strictEqual(a, b);
}

{
  let errToThrow;
  const thrower = promisify(function(a, b, c, cb) {
    errToThrow = new Error();
    throw errToThrow;
  });
  thrower(1, 2, 3)
    .then(assert.fail)
    .then(assert.fail, (e) => assert.strictEqual(e, errToThrow));
}

{
  const err = new Error();

  const a = promisify((cb) => cb(err))();
  const b = promisify(() => { throw err; })();

  Promise.all([
    a.then(assert.fail, function(e) {
      assert.strictEqual(err, e);
    }),
    b.then(assert.fail, function(e) {
      assert.strictEqual(err, e);
    }),
  ]);
}

[undefined, null, true, 0, 'str', {}, [], Symbol()].forEach((input) => {
  assert.throws(
    () => promisify(input),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
      message: 'The "original" argument must be of type function.' +
               common.invalidArgTypeHelper(input)
    });
});
