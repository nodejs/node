'use strict';
require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const fixtures = require('../common/fixtures');

// A `before` hook failure inside a suite marked `todo` must not fail the run.
// The `todo` flag on a suite signals that its results are advisory, the same
// way it does on individual tests. Before this fix, the suite branch of
// `countCompletedTest` flipped `harness.success` for any non-passing suite
// regardless of `isTodo`, so a hook failure exited with code 1 even though
// no failing tests were reported.
const child = spawnSync(process.execPath, [
  '--test',
  '--test-reporter=tap',
  fixtures.path('test-runner', 'todo-suite-failing-hook.mjs'),
]);

const stdout = child.stdout.toString();
assert.strictEqual(child.signal, null);
assert.strictEqual(child.status, 0,
                   `expected exit 0, got ${child.status}\nstdout:\n${stdout}\nstderr:\n${child.stderr}`);
assert.match(stdout, /# fail 0/);
assert.match(stdout, /# todo 2/);
