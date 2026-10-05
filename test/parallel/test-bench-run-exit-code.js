// Flags: --experimental-bench --no-warnings
'use strict';

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');

function runScript(script) {
  return spawnSync(process.execPath, [
    '--no-warnings',
    '--experimental-bench',
    '-e',
    script,
  ]);
}

// A failing benchmark consumed through the module-level run() sets the exit
// code.
{
  const child = runScript(`
    const { bench, run } = require('node:bench');
    bench('failure', () => { throw new Error('boom'); });
    (async () => { for await (const record of run()); })();
  `);
  assert.strictEqual(child.status, 1, child.stderr.toString());
}

// A successful run through the module-level run() keeps the default exit code.
{
  const child = runScript(`
    const { bench, run } = require('node:bench');
    bench('success', { samples: 1 }, (b) => {
      b.record({ duration_ns: 1n, operations: 1 });
    });
    (async () => { for await (const record of run()); })();
  `);
  assert.strictEqual(child.status, 0, child.stderr.toString());
}

// An exit code set by the user is preserved when the explicit run fails.
{
  const child = runScript(`
    const { bench, run } = require('node:bench');
    process.exitCode = 3;
    bench('failure', () => { throw new Error('boom'); });
    (async () => { for await (const record of run()); })();
  `);
  assert.strictEqual(child.status, 3, child.stderr.toString());
}

// An exit code set by the user is preserved when an automatic run fails.
{
  const child = runScript(`
    process.exitCode = 3;
    require('node:bench').bench('failure', () => { throw new Error(); });
  `);
  assert.strictEqual(child.status, 3, child.stderr.toString());
}

// Runners created with createRunner() leave the exit code to the caller.
{
  const child = runScript(`
    const runner = require('node:bench').createRunner();
    runner.bench('failure', () => { throw new Error('boom'); });
    (async () => { for await (const record of runner.run()); })();
  `);
  assert.strictEqual(child.status, 0, child.stderr.toString());
}
