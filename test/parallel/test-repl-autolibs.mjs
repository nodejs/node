import * as common from '../common/index.mjs';
import { startNewREPLServer } from '../common/repl.js';
import assert from 'node:assert';
import util from 'node:util';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);

const { output, run } = startNewREPLServer({ useGlobal: true, terminal: false });

// Referencing a builtin by bare name auto-requires it onto the real global,
// and the value is identity-equal to the normally-required module.
{
  output.accumulator = '';
  await run(['fs']);
  assert.strictEqual(output.accumulator,
                     `${util.inspect(require('fs'), null, 2, false)}\n`);
  assert.strictEqual(globalThis.fs, require('fs'));
}

// A user-defined global with a builtin's name is NOT overwritten by the
// auto-require, and the REPL prints the user's value.
{
  const val = {};
  globalThis.url = val;
  common.allowGlobals(val);

  output.accumulator = '';
  await run(['url']);
  assert.strictEqual(output.accumulator, '{}\n');
  assert.strictEqual(val, globalThis.url);
}
