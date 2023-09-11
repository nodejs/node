'use strict';

// Tests invalid --heap-prof CLI arguments.

const common = require('../common');

const fixtures = require('../common/fixtures');
common.skipIfInspectorDisabled();

const assert = require('assert');
const { spawnSync } = require('child_process');

const tmpdir = require('../common/tmpdir');

const {
  kHeapProfInterval,
  env
} = require('../common/prof');

// Tests --heap-prof-name without --heap-prof.
{
  tmpdir.refresh();
  const output = spawnSync(process.execPath, [
    '--heap-prof-name',
    'test.heapprofile',
    fixtures.path('workload', 'allocation.js'),
  ], {
    cwd: tmpdir.path,
    env
  });
  const stderr = output.stderr.toString().trim();
  if (output.status !== 9) {
    console.log(stderr);
  }
  assert.strictEqual(output.status, 9);
  assert.strictEqual(
    stderr,
    `${process.execPath}: --heap-prof-name must be used with --heap-prof`);
}

// Tests --heap-prof-dir without --heap-prof.
{
  tmpdir.refresh();
  const output = spawnSync(process.execPath, [
    '--heap-prof-dir',
    'prof',
    fixtures.path('workload', 'allocation.js'),
  ], {
    cwd: tmpdir.path,
    env
  });
  const stderr = output.stderr.toString().trim();
  if (output.status !== 9) {
    console.log(stderr);
  }
  assert.strictEqual(output.status, 9);
  assert.strictEqual(
    stderr,
    `${process.execPath}: --heap-prof-dir must be used with --heap-prof`);
}

// Tests --heap-prof-interval without --heap-prof.
{
  tmpdir.refresh();
  const output = spawnSync(process.execPath, [
    '--heap-prof-interval',
    kHeapProfInterval,
    fixtures.path('workload', 'allocation.js'),
  ], {
    cwd: tmpdir.path,
    env
  });
  const stderr = output.stderr.toString().trim();
  if (output.status !== 9) {
    console.log(stderr);
  }
  assert.strictEqual(output.status, 9);
  assert.strictEqual(
    stderr,
    `${process.execPath}: ` +
    '--heap-prof-interval must be used with --heap-prof');
}
