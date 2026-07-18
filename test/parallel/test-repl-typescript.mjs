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

// Syntax which is valid in both languages must retain its JavaScript
// semantics instead of being type-stripped.
await run("const foo = () => 'hello world'; foo<Object>(0);\n");
assert.match(output.accumulator, /false\n> /);
