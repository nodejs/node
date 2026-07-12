'use strict';

const common = require('../common');
const assert = require('assert');
const { startNewREPLServer } = require('../common/repl');

common.skipIfInspectorDisabled();

const { output, input } = startNewREPLServer();

output.accumulator = '';

// Input without '\n' triggering actual run.
const inputStr = 'while (true) {}';
input.emit('data', inputStr);
// No preview available when timed out.
assert.strictEqual(output.accumulator, inputStr);
