'use strict';
const common = require('../common');
const assert = require('assert');
const { isNativeError } = require('util/types');
const { Worker } = require('worker_threads');

{
  const w = new Worker('throw new Error()', { eval: true });
  w.on('error', common.mustCall((error) => {
    assert(isNativeError(error));
    assert.strictEqual(error.constructor, Error);
    assert.strictEqual(Object.getPrototypeOf(error), Error.prototype);
  }));
}

{
  const w = new Worker('throw new RangeError()', { eval: true });
  w.on('error', common.mustCall((error) => {
    assert(isNativeError(error));
    assert.strictEqual(error.constructor, RangeError);
    assert.strictEqual(Object.getPrototypeOf(error), RangeError.prototype);
  }));
}

{
  const w = new Worker('throw new Error(undefined, { cause: new TypeError() })', { eval: true });
  w.on('error', common.mustCall((error) => {
    assert(isNativeError(error));
    assert.strictEqual(error.constructor, Error);
    assert.strictEqual(Object.getPrototypeOf(error), Error.prototype);

    assert(isNativeError(error.cause));
    assert.strictEqual(error.cause.constructor, TypeError);
    assert.strictEqual(Object.getPrototypeOf(error.cause), TypeError.prototype);
  }));
}
