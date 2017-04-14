'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const vm = require('vm');
const { promisify } = require('util');

common.crashOnUnhandledRejection();

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
  fn[promisify.custom] = 42;
  assert.throws(
      () => promisify(fn),
      (err) => err instanceof TypeError &&
                err.message === 'The [util.promisify.custom] property must ' +
                                'be a function');
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
    assert.deepStrictEqual(value, 'foo');
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
