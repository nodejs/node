// Flags: --experimental-vm-modules
'use strict';

// This tests the result of evaluating a vm.SynthethicModule.
// See https://tc39.es/ecma262/#sec-smr-Evaluate
const common = require('../common');

const assert = require('assert');
// To make testing easier we just use the public inspect API. If the output format
// changes, update this test accordingly.
const { inspect } = require('util');
const vm = require('vm');

// Synthetic modules with a synchronous evaluation step evaluate to a promise synchronously
// resolved to undefined.
{
  const mod = new vm.SyntheticModule(['a'], common.mustCall(() => {
    mod.setExport('a', 42);
  }));
  const promise = mod.evaluate();
  assert.match(inspect(promise), /Promise { undefined }/);
  assert.strictEqual(mod.namespace.a, 42);

  promise.then(common.mustCall((value) => {
    assert.strictEqual(value, undefined);

    // Calling evaluate() again results in a promise synchronously resolved to undefined.
    const promise2 = mod.evaluate();
    assert.match(inspect(promise2), /Promise { undefined }/);
    promise2.then(common.mustCall((value) => {
      assert.strictEqual(value, undefined);
    }));
  }));
}

// Synthetic modules with an asynchronous evaluation step evaluate to a promise
// _synchronously_ resolved to undefined.
{
  const mod = new vm.SyntheticModule(['a'], common.mustCall(async () => {
    const result = await Promise.resolve(42);
    mod.setExport('a', result);
    return result;
  }));
  const promise = mod.evaluate();
  assert.match(inspect(promise), /Promise { undefined }/);
  // Accessing the uninitialized export of a synthetic module returns undefined.
  assert.strictEqual(mod.namespace.a, undefined);

  promise.then(common.mustCall((value) => {
    assert.strictEqual(value, undefined);

    // Calling evaluate() again results in a promise _synchronously_ resolved to undefined again.
    const promise2 = mod.evaluate();
    assert.match(inspect(promise2), /Promise { undefined }/);
    promise2.then(common.mustCall((value) => {
      assert.strictEqual(value, undefined);
    }));
  }));
}

// Synchronous error during the evaluation step of a synthetic module results
// in a _synchronously_ rejected promise.
{
  const mod = new vm.SyntheticModule(['a'], common.mustCall(() => {
    throw new Error('synchronous synthethic module');
  }));
  const promise = mod.evaluate();
  assert.match(inspect(promise), /rejected/);
  assert(mod.error, 'Expected mod.error to be set');
  assert.strictEqual(mod.error.message, 'synchronous synthethic module');

  promise.catch(common.mustCall((err) => {
    assert.strictEqual(err, mod.error);

    // Calling evaluate() again results in a promise _synchronously_ rejected
    // with the same reason.
    const promise2 = mod.evaluate();
    assert.match(inspect(promise2), /rejected/);
    promise2.catch(common.mustCall((err2) => {
      assert.strictEqual(err, err2);
    }));
  }));
}
