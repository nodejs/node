'use strict';

const common = require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');

const spawnTimeout = common.platformTimeout(30_000);

function spawnNode(args, options = undefined) {
  const result = spawnSync(process.execPath, args, {
    __proto__: null,
    encoding: 'utf8',
    timeout: spawnTimeout,
    ...options,
  });
  assert.ifError(result.error);
  assert.strictEqual(result.signal, null);
  return result;
}

function spawnBench(args, options = undefined) {
  return spawnNode([
    '--no-warnings', '--experimental-bench', '--bench', ...args,
  ], options);
}

for (const { args, message } of [
  {
    args: ['--bench-isolation=invalid', 'unused.js'],
    message: /invalid value for --bench-isolation/,
  },
  {
    args: ['--bench-samples=0', 'unused.js'],
    message: /--bench-samples must be greater than 0/,
  },
  {
    args: ['--bench-warmup=4294967296', 'unused.js'],
    message: /--bench-warmup.*out of range/,
  },
  {
    args: ['--bench-samples=2x', 'unused.js'],
    message: /invalid value for --bench-samples/,
  },
  {
    args: ['--bench-warmup=abc', 'unused.js'],
    message: /invalid value for --bench-warmup/,
  },
  {
    args: ['--bench-name-pattern=[', 'unused.js'],
    message: /invalid regular expression/,
  },
  {
    args: ['--eval=1', 'unused.js'],
    message: /either --bench or --eval can be used, not both/,
  },
  {
    args: ['--interactive', 'unused.js'],
    message: /either --bench or --interactive can be used, not both/,
  },
  {
    args: ['--watch', 'unused.js'],
    message: /either --bench or --watch can be used, not both/,
  },
  {
    args: ['--watch-path=.', 'unused.js'],
    message: /either --bench or --watch can be used, not both/,
  },
  {
    args: ['--check', 'unused.js'],
    message: /either --bench or --check can be used, not both/,
  },
  {
    args: ['--test', 'unused.js'],
    message: /either --bench or --test can be used, not both/,
  },
]) {
  const result = spawnBench(args);
  assert.notStrictEqual(result.status, 0);
  assert.match(result.stderr, message);
}
