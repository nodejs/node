// Flags: --experimental-vm-modules
'use strict';

// This tests the result of evaluating a vm.SourceTextModule.
const common = require('../common');

const assert = require('assert');
// To make testing easier we just use the public inspect API. If the output format
// changes, update this test accordingly.
const { inspect } = require('util');
const vm = require('vm');

globalThis.callCount = {};
common.allowGlobals(globalThis.callCount);

// Synchronous error during evaluation results in a synchronously rejected promise.
{
  globalThis.callCount.syncError = 0;
  const mod = new vm.SourceTextModule(`
    globalThis.callCount.syncError++;
    throw new Error("synchronous source text module");
    export const a = 1;
  `);
  mod.linkRequests([]);
  mod.instantiate();
  const promise = mod.evaluate();
  assert.strictEqual(globalThis.callCount.syncError, 1);
  assert.match(inspect(promise), /rejected/);
  assert(mod.error, 'Expected mod.error to be set');
  assert.strictEqual(mod.error.message, 'synchronous source text module');

  promise.catch(common.mustCall((err) => {
    assert.strictEqual(err, mod.error);
    // Calling evaluate() again results in the same rejection synchronously.
    const promise2 = mod.evaluate();
    assert.match(inspect(promise2), /rejected/);
    promise2.catch(common.mustCall((err2) => {
      assert.strictEqual(err, err2);
      // The module is only evaluated once.
      assert.strictEqual(globalThis.callCount.syncError, 1);
    }));
  }));
}

// Successful evaluation of a module without top-level await results in a
// promise synchronously resolved to undefined.
{
  globalThis.callCount.syncNamedExports = 0;
  const mod = new vm.SourceTextModule(`
    globalThis.callCount.syncNamedExports++;
    export const a = 1, b = 2;
  `);
  mod.linkRequests([]);
  mod.instantiate();
  const promise = mod.evaluate();
  assert.match(inspect(promise), /Promise { undefined }/);
  assert.strictEqual(mod.namespace.a, 1);
  assert.strictEqual(mod.namespace.b, 2);
  assert.strictEqual(globalThis.callCount.syncNamedExports, 1);
  promise.then(common.mustCall((value) => {
    assert.strictEqual(value, undefined);

    // Calling evaluate() again results in the same resolved promise synchronously.
    const promise2 = mod.evaluate();
    assert.match(inspect(promise2), /Promise { undefined }/);
    assert.strictEqual(mod.namespace.a, 1);
    assert.strictEqual(mod.namespace.b, 2);
    promise2.then(common.mustCall((value) => {
      assert.strictEqual(value, undefined);
      // The module is only evaluated once.
      assert.strictEqual(globalThis.callCount.syncNamedExports, 1);
    }));
  }));
}

{
  globalThis.callCount.syncDefaultExports = 0;
  // Modules with either named and default exports have the same behaviors.
  const mod = new vm.SourceTextModule(`
    globalThis.callCount.syncDefaultExports++;
    export default 42;
  `);
  mod.linkRequests([]);
  mod.instantiate();
  const promise = mod.evaluate();
  assert.match(inspect(promise), /Promise { undefined }/);
  assert.strictEqual(mod.namespace.default, 42);
  assert.strictEqual(globalThis.callCount.syncDefaultExports, 1);

  promise.then(common.mustCall((value) => {
    assert.strictEqual(value, undefined);

    // Calling evaluate() again results in the same resolved promise synchronously.
    const promise2 = mod.evaluate();
    assert.match(inspect(promise2), /Promise { undefined }/);
    assert.strictEqual(mod.namespace.default, 42);
    promise2.then(common.mustCall((value) => {
      assert.strictEqual(value, undefined);
      // The module is only evaluated once.
      assert.strictEqual(globalThis.callCount.syncDefaultExports, 1);
    }));
  }));
}

// Successful evaluation of a module with top-level await results in a promise
// that is fulfilled asynchronously with undefined.
{
  globalThis.callCount.asyncEvaluation = 0;
  const mod = new vm.SourceTextModule(`
    globalThis.callCount.asyncEvaluation++;
    await Promise.resolve();
    export const a = 1;
  `);
  mod.linkRequests([]);
  mod.instantiate();
  const promise = mod.evaluate();
  assert.match(inspect(promise), /<pending>/);
  // Accessing the namespace before the promise is fulfilled throws ReferenceError.
  assert.throws(() => mod.namespace.a, { name: 'ReferenceError' });
  assert.strictEqual(globalThis.callCount.asyncEvaluation, 1);
  promise.then(common.mustCall((value) => {
    assert.strictEqual(value, undefined);
    assert.strictEqual(globalThis.callCount.asyncEvaluation, 1);

    // Calling evaluate() again results in a promise synchronously resolved to undefined.
    const promise2 = mod.evaluate();
    assert.match(inspect(promise2), /Promise { undefined }/);
    assert.strictEqual(mod.namespace.a, 1);
    promise2.then(common.mustCall((value) => {
      assert.strictEqual(value, undefined);
      // The module is only evaluated once.
      assert.strictEqual(globalThis.callCount.asyncEvaluation, 1);
    }));
  }));
}

// Rejection of a top-level await promise results in a promise that is
// rejected asynchronously with the same reason.
{
  globalThis.callCount.asyncRejection = 0;
  const mod = new vm.SourceTextModule(`
    globalThis.callCount.asyncRejection++;
    await Promise.reject(new Error("asynchronous source text module"));
    export const a = 1;
  `);
  mod.linkRequests([]);
  mod.instantiate();
  const promise = mod.evaluate();
  assert.match(inspect(promise), /<pending>/);
  // Accessing the namespace before the promise is fulfilled throws ReferenceError.
  assert.throws(() => mod.namespace.a, { name: 'ReferenceError' });
  promise.catch(common.mustCall((err) => {
    assert.strictEqual(err, mod.error);
    assert.strictEqual(err.message, 'asynchronous source text module');
    assert.strictEqual(globalThis.callCount.asyncRejection, 1);

    // Calling evaluate() again results in a promise synchronously rejected
    // with the same reason.
    const promise2 = mod.evaluate();
    assert.match(inspect(promise2), /rejected/);
    promise2.catch(common.mustCall((err2) => {
      assert.strictEqual(err, err2);
      // The module is only evaluated once.
      assert.strictEqual(globalThis.callCount.asyncRejection, 1);
    }));
  }));
}
