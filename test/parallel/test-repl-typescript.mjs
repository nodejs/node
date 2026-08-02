// Flags: --experimental-repl-typescript

import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { startNewREPLServer } from '../common/repl.js';

common.skipIfInspectorDisabled();

const { run, output } = startNewREPLServer({
  terminal: false,
  prompt: '> ',
});

await run('let x: number = 3\n');
assert.match(output.accumulator, /undefined\n> /);
output.accumulator = '';

await run('x\n');
assert.match(output.accumulator, /3\n> /);
output.accumulator = '';
