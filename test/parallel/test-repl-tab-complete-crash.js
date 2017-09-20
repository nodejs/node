'use strict';

const common = require('../common');
const assert = require('assert');
const repl = require('repl');

common.ArrayStream.prototype.write = () => {};

const putIn = new common.ArrayStream();
const testMe = repl.start('', putIn);

// https://github.com/nodejs/node/issues/3346
// Tab-completion should be empty
putIn.run(['.clear']);
putIn.run(['function () {']);
testMe.complete('arguments.', common.mustCall((err, completions) => {
  assert.strictEqual(err, null);
  assert.deepStrictEqual(completions, [[], 'arguments.']);
}));

putIn.run(['.clear']);
putIn.run(['function () {']);
putIn.run(['undef;']);
testMe.complete('undef.', common.mustCall((err, completions) => {
  assert.strictEqual(err, null);
  assert.deepStrictEqual(completions, [[], 'undef.']);
}));
