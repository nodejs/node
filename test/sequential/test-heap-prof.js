'use strict';

// This tests that --heap-prof, --heap-prof-dir and --heap-prof-name works.

const common = require('../common');

const fixtures = require('../common/fixtures');
common.skipIfInspectorDisabled();

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');

const tmpdir = require('../common/tmpdir');

function getHeapProfiles(dir) {
  const list = fs.readdirSync(dir);
  return list
    .filter((file) => file.endsWith('.heapprofile'))
    .map((file) => path.join(dir, file));
}

function findFirstFrameInNode(root, func) {
  const first = root.children.find(
    (child) => child.callFrame.functionName === func
  );
  if (first) {
    return first;
  }
  for (const child of root.children) {
    const first = findFirstFrameInNode(child, func);
    if (first) {
      return first;
    }
  }
  return undefined;
}

function findFirstFrame(file, func) {
  const data = fs.readFileSync(file, 'utf8');
  const profile = JSON.parse(data);
  const first = findFirstFrameInNode(profile.head, func);
  return { frame: first, roots: profile.head.children };
}

function verifyFrames(output, file, func) {
  const { frame, roots } = findFirstFrame(file, func);
  if (!frame) {
    // Show native debug output and the profile for debugging.
    console.log(output.stderr.toString());
    console.log(roots);
  }
  assert.notDeepStrictEqual(frame, undefined);
}

// We need to set --heap-prof-interval to a small enough value to make
// sure we can find our workload in the samples, so we need to set
// TEST_ALLOCATION > kHeapProfInterval.
const kHeapProfInterval = 128;
const TEST_ALLOCATION = kHeapProfInterval * 2;

const env = {
  ...process.env,
  TEST_ALLOCATION,
  NODE_DEBUG_NATIVE: 'INSPECTOR_PROFILER'
};

// Test --heap-prof without --heap-prof-interval. Here we just verify that
// we manage to generate a profile.
{
  tmpdir.refresh();
  const output = spawnSync(process.execPath, [
    '--heap-prof',
    fixtures.path('workload', 'allocation.js'),
  ], {
    cwd: tmpdir.path,
    env
  });
  if (output.status !== 0) {
    console.log(output.stderr.toString());
    console.log(output);
  }
  assert.strictEqual(output.status, 0);
  const profiles = getHeapProfiles(tmpdir.path);
  assert.strictEqual(profiles.length, 1);
}

// Outputs heap profile when event loop is drained.
// TODO(joyeecheung): share the fixutres with v8 coverage tests
{
  tmpdir.refresh();
  const output = spawnSync(process.execPath, [
    '--heap-prof',
    '--heap-prof-interval',
    kHeapProfInterval,
    fixtures.path('workload', 'allocation.js'),
  ], {
    cwd: tmpdir.path,
    env
  });
  if (output.status !== 0) {
    console.log(output.stderr.toString());
    console.log(output);
  }
  assert.strictEqual(output.status, 0);
  const profiles = getHeapProfiles(tmpdir.path);
  assert.strictEqual(profiles.length, 1);
  verifyFrames(output, profiles[0], 'runAllocation');
}

// Outputs heap profile when process.exit(55) exits process.
{
  tmpdir.refresh();
  const output = spawnSync(process.execPath, [
    '--heap-prof',
    '--heap-prof-interval',
    kHeapProfInterval,
    fixtures.path('workload', 'allocation-exit.js'),
  ], {
    cwd: tmpdir.path,
    env
  });
  if (output.status !== 55) {
    console.log(output.stderr.toString());
  }
  assert.strictEqual(output.status, 55);
  const profiles = getHeapProfiles(tmpdir.path);
  assert.strictEqual(profiles.length, 1);
  verifyFrames(output, profiles[0], 'runAllocation');
}

// Outputs heap profile when process.kill(process.pid, "SIGINT"); exits process.
{
  tmpdir.refresh();
  const output = spawnSync(process.execPath, [
    '--heap-prof',
    '--heap-prof-interval',
    kHeapProfInterval,
    fixtures.path('workload', 'allocation-sigint.js'),
  ], {
    cwd: tmpdir.path,
    env
  });
  if (!common.isWindows) {
    if (output.signal !== 'SIGINT') {
      console.log(output.stderr.toString());
    }
    assert.strictEqual(output.signal, 'SIGINT');
  }
  const profiles = getHeapProfiles(tmpdir.path);
  assert.strictEqual(profiles.length, 1);
  verifyFrames(output, profiles[0], 'runAllocation');
}

// Outputs heap profile from worker when execArgv is set.
{
  tmpdir.refresh();
  const output = spawnSync(process.execPath, [
    fixtures.path('workload', 'allocation-worker-argv.js'),
  ], {
    cwd: tmpdir.path,
    env: {
      ...process.env,
      HEAP_PROF_INTERVAL: '128'
    }
  });
  if (output.status !== 0) {
    console.log(output.stderr.toString());
  }
  assert.strictEqual(output.status, 0);
  const profiles = getHeapProfiles(tmpdir.path);
  assert.strictEqual(profiles.length, 1);
  verifyFrames(output, profiles[0], 'runAllocation');
}

// --heap-prof-name without --heap-prof
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

// --heap-prof-dir without --heap-prof
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

// --heap-prof-interval without --heap-prof
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

// --heap-prof-name
{
  tmpdir.refresh();
  const file = path.join(tmpdir.path, 'test.heapprofile');
  const output = spawnSync(process.execPath, [
    '--heap-prof',
    '--heap-prof-name',
    'test.heapprofile',
    '--heap-prof-interval',
    kHeapProfInterval,
    fixtures.path('workload', 'allocation.js'),
  ], {
    cwd: tmpdir.path,
    env
  });
  if (output.status !== 0) {
    console.log(output.stderr.toString());
  }
  assert.strictEqual(output.status, 0);
  const profiles = getHeapProfiles(tmpdir.path);
  assert.deepStrictEqual(profiles, [file]);
  verifyFrames(output, file, 'runAllocation');
}

// relative --heap-prof-dir
{
  tmpdir.refresh();
  const output = spawnSync(process.execPath, [
    '--heap-prof',
    '--heap-prof-dir',
    'prof',
    '--heap-prof-interval',
    kHeapProfInterval,
    fixtures.path('workload', 'allocation.js'),
  ], {
    cwd: tmpdir.path,
    env
  });
  if (output.status !== 0) {
    console.log(output.stderr.toString());
  }
  assert.strictEqual(output.status, 0);
  const dir = path.join(tmpdir.path, 'prof');
  assert(fs.existsSync(dir));
  const profiles = getHeapProfiles(dir);
  assert.strictEqual(profiles.length, 1);
  verifyFrames(output, profiles[0], 'runAllocation');
}

// absolute --heap-prof-dir
{
  tmpdir.refresh();
  const dir = path.join(tmpdir.path, 'prof');
  const output = spawnSync(process.execPath, [
    '--heap-prof',
    '--heap-prof-dir',
    dir,
    '--heap-prof-interval',
    kHeapProfInterval,
    fixtures.path('workload', 'allocation.js'),
  ], {
    cwd: tmpdir.path,
    env
  });
  if (output.status !== 0) {
    console.log(output.stderr.toString());
  }
  assert.strictEqual(output.status, 0);
  assert(fs.existsSync(dir));
  const profiles = getHeapProfiles(dir);
  assert.strictEqual(profiles.length, 1);
  verifyFrames(output, profiles[0], 'runAllocation');
}

// --heap-prof-dir and --heap-prof-name
{
  tmpdir.refresh();
  const dir = path.join(tmpdir.path, 'prof');
  const file = path.join(dir, 'test.heapprofile');
  const output = spawnSync(process.execPath, [
    '--heap-prof',
    '--heap-prof-name',
    'test.heapprofile',
    '--heap-prof-dir',
    dir,
    '--heap-prof-interval',
    kHeapProfInterval,
    fixtures.path('workload', 'allocation.js'),
  ], {
    cwd: tmpdir.path,
    env
  });
  if (output.status !== 0) {
    console.log(output.stderr.toString());
  }
  assert.strictEqual(output.status, 0);
  assert(fs.existsSync(dir));
  const profiles = getHeapProfiles(dir);
  assert.deepStrictEqual(profiles, [file]);
  verifyFrames(output, file, 'runAllocation');
}

{
  tmpdir.refresh();
  const output = spawnSync(process.execPath, [
    '--heap-prof-interval',
    kHeapProfInterval,
    '--heap-prof-dir',
    'prof',
    '--heap-prof',
    fixtures.path('workload', 'allocation-worker.js'),
  ], {
    cwd: tmpdir.path,
    env
  });
  if (output.status !== 0) {
    console.log(output.stderr.toString());
  }
  assert.strictEqual(output.status, 0);
  const dir = path.join(tmpdir.path, 'prof');
  assert(fs.existsSync(dir));
  const profiles = getHeapProfiles(dir);
  assert.strictEqual(profiles.length, 2);
  const profile1 = findFirstFrame(profiles[0], 'runAllocation');
  const profile2 = findFirstFrame(profiles[1], 'runAllocation');
  if (!profile1.frame && !profile2.frame) {
    // Show native debug output and the profile for debugging.
    console.log(output.stderr.toString());
    console.log('heap path: ', profiles[0]);
    console.log(profile1.roots);
    console.log('heap path: ', profiles[1]);
    console.log(profile2.roots);
  }
  assert(profile1.frame || profile2.frame);
}
