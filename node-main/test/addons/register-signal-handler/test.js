'use strict';
const common = require('../../common');
if (common.isWindows)
  common.skip('No RegisterSignalHandler() on Windows');

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { signals } = require('os').constants;
const { spawnSync } = require('child_process');

const bindingPath = path.resolve(
  __dirname, 'build', common.buildType, 'binding.node');

if (!fs.existsSync(bindingPath))
  common.skip('binding not built yet');

const binding = require(bindingPath);

if (process.argv[2] === 'child') {
  const signo = +process.argv[3];
  const reset = process.argv[4] === 'reset';
  const count = +process.argv[5];

  binding.registerSignalHandler(signo, reset);
  for (let i = 0; i < count; i++)
    process.kill(process.pid, signo);
  return;
}

for (const raiseSignal of [ 'SIGABRT', 'SIGSEGV' ]) {
  const signo = signals[raiseSignal];
  for (const { reset, count, stderr, code, signal } of [
    { reset: true, count: 1, stderr: [signo], code: 0, signal: null },
    { reset: true, count: 2, stderr: [signo], code: null, signal: raiseSignal },
    { reset: false, count: 1, stderr: [signo], code: 0, signal: null },
    { reset: false, count: 2, stderr: [signo, signo], code: 0, signal: null },
  ]) {
    // We do not want to generate core files when running this test as an
    // addon test. We require this file as an abort test as well, though,
    // with ALLOW_CRASHES set.
    if (signal !== null && !process.env.ALLOW_CRASHES)
      continue;
    // reset_handler does not work with SIGSEGV.
    if (reset && signo === signals.SIGSEGV)
      continue;

    const args = [__filename, 'child', signo, reset ? 'reset' : '', count];
    console.log(`Running: node ${args.join(' ')}`);
    const result = spawnSync(
      process.execPath, args, { stdio: ['inherit', 'pipe', 'inherit'] });
    assert.strictEqual(result.status, code);
    assert.strictEqual(result.signal, signal);
    assert.deepStrictEqual([...result.stdout], stderr);
  }
}
