// Flags: --experimental-repl-typescript
'use strict';
require('../common');
const assert = require('assert');

const { startNewREPLServer } = require('../common/repl');

const { input, output } = startNewREPLServer({ terminal: false, prompt: '> ' });

input.emit('data', 'let x: number = 3\n');
assert.match(output.accumulator, /undefined\n> /);
output.accumulator = '';

input.emit('data', 'x\n');
assert.match(output.accumulator, /3\n> /);
