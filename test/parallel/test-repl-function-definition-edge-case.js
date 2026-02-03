// Reference: https://github.com/nodejs/node/pull/7624
'use strict';
const common = require('../common');
const assert = require('assert');
const { startNewREPLServer } = require('../common/repl');

const { replServer } = startNewREPLServer({
  useColors: false,
  terminal: false
});

replServer.input.emit('data', 'function a() { return 42; } (1)\n');
replServer.input.emit('data', 'a\n');
replServer.input.emit('data', '.exit\n');
replServer.once('exit', common.mustCall());

const expected = '1\n[Function: a]\n';
const got = replServer.output.accumulator;
assert.strictEqual(got, expected);
