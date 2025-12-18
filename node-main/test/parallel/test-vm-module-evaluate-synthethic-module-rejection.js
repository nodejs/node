// Flags: --experimental-vm-modules
'use strict';

// This tests the result of evaluating a vm.SyntheticModule with an async rejection
// in the evaluation step.

const common = require('../common');

const assert = require('assert');
// To make testing easier we just use the public inspect API. If the output format
// changes, update this test accordingly.
const { inspect } = require('util');
const vm = require('vm');

// The promise _synchronously_ resolves to undefined, because for a synthethic module,
// the evaluation operation can only either resolve or reject immediately.
// In this case, the asynchronously rejected promise can't be handled from the outside,
// so we'll catch it with the isolate-level unhandledRejection handler.
// See https://tc39.es/ecma262/#sec-smr-Evaluate
process.on('unhandledRejection', common.mustCall((err) => {
  assert.strictEqual(err.message, 'asynchronous source text module');
}));

const mod = new vm.SyntheticModule(['a'], common.mustCall(async () => {
  throw new Error('asynchronous source text module');
}));

const promise = mod.evaluate();
assert.match(inspect(promise), /Promise { undefined }/);
// Accessing the uninitialized export of a synthetic module returns undefined.
assert.strictEqual(mod.namespace.a, undefined);

promise.then(common.mustCall((value) => {
  assert.strictEqual(value, undefined);
}));

// Calling evaluate() again results in a promise _synchronously_ resolved to undefined again.
const promise2 = mod.evaluate();
assert.match(inspect(promise2), /Promise { undefined }/);
promise2.then(common.mustCall((value) => {
  assert.strictEqual(value, undefined);
}));
