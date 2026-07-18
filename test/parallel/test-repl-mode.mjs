import * as common from '../common/index.mjs';
import assert from 'node:assert';
import repl from 'node:repl';
import { startNewREPLServer } from '../common/repl.js';

if (process.env.TERM === 'dumb') {
  common.skip('skipping - dumb terminal');
}

const tests = [testSloppyMode, testStrictMode, testStrictModeTerminal];

async function testSloppyMode() {
  const { run, output } = startNewREPLServer({
    replMode: repl.REPL_MODE_SLOPPY,
    terminal: false,
    prompt: '> ',
  });

  await run('x = 3\n');
  assert.strictEqual(output.accumulator, '> 3\n> ');
  output.accumulator = '';

  await run('let y = 3\n');
  assert.strictEqual(output.accumulator, 'undefined\n> ');
}

async function testStrictMode() {
  const { run, output } = startNewREPLServer({
    replMode: repl.REPL_MODE_STRICT,
    terminal: false,
    prompt: '> ',
  });

  await run('x = 3\n');
  assert.match(output.accumulator, /ReferenceError: x is not defined/);
  output.accumulator = '';

  await run('let y = 3\n');
  assert.strictEqual(output.accumulator, 'undefined\n> ');
}

async function testStrictModeTerminal() {
  // Verify that ReferenceErrors are reported in strict mode previews.
  const { run, output } = startNewREPLServer({
    replMode: repl.REPL_MODE_STRICT,
    prompt: '> ',
  });

  await run('xyz ');
  assert.ok(
    output.accumulator.includes('\n// ReferenceError: xyz is not defined'),
  );
}

for (const test of tests) {
  await test();
}
