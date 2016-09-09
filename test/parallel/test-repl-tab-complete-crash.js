'use strict';

const common = require('../common');
const assert = require('assert');
const repl = require('repl');

common.ArrayStream.prototype.write = common.noop;

const runAsHuman = (cmd, repl) => {
  const cmds = Array.isArray(cmd) ? cmd : [cmd];
  repl.input.run(cmds);
  repl._sawKeyPress = true;
  repl.input.emit('keypress', null, { name: 'return' });
};

const clear = (repl) => {
  repl.input.emit('keypress', null, { name: 'c', ctrl: true});
  runAsHuman('.clear', repl);
};

const putIn = new common.ArrayStream();
const testMe = repl.start({
  input: putIn,
  output: putIn,
  terminal: true
});
// Test cases for default tab completion
testMe.complete = testMe.defaultComplete;

// https://github.com/nodejs/node/issues/3346
// Tab-completion should be empty
clear(testMe);
putIn.run(['function () {']);
testMe.complete('arguments.', common.mustCall((err, completions) => {
  assert.strictEqual(err, null);
  assert.deepStrictEqual(completions, [[], 'arguments.']);
}));

clear(testMe);
putIn.run(['function () {', 'undef;']);
testMe.complete('undef.', common.mustCall((err, completions) => {
  assert.strictEqual(err, null);
  assert.deepStrictEqual(completions, [[], 'undef.']);
}));
