'use strict';

const common = require('../common');
const assert = require('assert');
const { startNewREPLServer } = require('../common/repl');

const { replServer, input } = startNewREPLServer({}, { disableDomainErrorAssert: true });

// https://github.com/nodejs/node/issues/3346
// Tab-completion should be empty
input.run(['.clear', 'function () {']);
replServer.complete('arguments.', common.mustCall((err, completions) => {
  assert.strictEqual(err, null);
  assert.deepStrictEqual(completions, [[], 'arguments.']);
}));

input.run(['.clear', 'function () {', 'undef;']);
replServer.complete('undef.', common.mustCall((err, completions) => {
  assert.strictEqual(err, null);
  assert.deepStrictEqual(completions, [[], 'undef.']);
}));
