'use strict';
const common = require('../common');
const assert = require('assert');
const { isNativeError } = require('util/types');
const { Worker } = require('worker_threads');

const validateError = (error, ctor) => {
  assert.strictEqual(error.constructor, ctor);
  assert.strictEqual(Object.getPrototypeOf(error), ctor.prototype);
  assert(isNativeError(error));
};

{
  const w = new Worker('throw new Error()', { eval: true });
  w.on('error', common.mustCall((error) => {
    validateError(error, Error);
  }));
}

{
  const w = new Worker('throw new RangeError()', { eval: true });
  w.on('error', common.mustCall((error) => {
    validateError(error, RangeError);
  }));
}

{
  const w = new Worker('throw new Error(undefined, { cause: new TypeError() })', { eval: true });
  w.on('error', common.mustCall((error) => {
    validateError(error, Error);
    assert.notStrictEqual(error.cause, undefined);
    validateError(error.cause, TypeError);
  }));
}
