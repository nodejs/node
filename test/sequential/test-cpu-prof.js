'use strict';

// This tests that --cpu-prof, --cpu-prof-dir and --cpu-prof-name works.

const common = require('../common');
const fixtures = require('../common/fixtures');
common.skipIfInspectorDisabled();

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');

const tmpdir = require('../common/tmpdir');

function getCpuProfiles(dir) {
  const list = fs.readdirSync(dir);
  return list
    .filter((file) => file.endsWith('.cpuprofile'))
    .map((file) => path.join(dir, file));
}

function getFrames(output, file, suffix) {
  const data = fs.readFileSync(file, 'utf8');
  const profile = JSON.parse(data);
  const frames = profile.nodes.filter((i) => {
    const frame = i.callFrame;
    return frame.url.endsWith(suffix);
  });
  return { frames, nodes: profile.nodes };
}

function verifyFrames(output, file, suffix) {
  const { frames, nodes } = getFrames(output, file, suffix);
  if (frames.length === 0) {
    // Show native debug output and the profile for debugging.
    console.log(output.stderr.toString());
    console.log(nodes);
  }
  assert.notDeepStrictEqual(frames, []);
}

let FIB = 30;
// This is based on emperial values - in the CI, on Windows the program
// tend to finish too fast then we won't be able to see the profiled script
// in the samples, so we need to bump the values a bit. On slower platforms
// like the Pis it could take more time to complete, we need to use a
// smaller value so the test would not time out.
if (common.isWindows) {
  FIB = 40;
}

// We need to set --cpu-interval to a smaller value to make sure we can
// find our workload in the samples. 50us should be a small enough sampling
// interval for this.
const kCpuProfInterval = 50;
const env = {
  ...process.env,
  FIB,
  NODE_DEBUG_NATIVE: 'INSPECTOR_PROFILER'
};

// Test --cpu-prof without --cpu-prof-interval. Here we just verify that
// we manage to generate a profile.
{
  tmpdir.refresh();
  const output = spawnSync(process.execPath, [
    '--cpu-prof',
    fixtures.path('workload', 'fibonacci.js'),
  ], {
    cwd: tmpdir.path,
    env
  });
  if (output.status !== 0) {
    console.log(output.stderr.toString());
  }
  assert.strictEqual(output.status, 0);
  const profiles = getCpuProfiles(tmpdir.path);
  assert.strictEqual(profiles.length, 1);
}

// Outputs CPU profile when event loop is drained.
// TODO(joyeecheung): share the fixutres with v8 coverage tests
{
  tmpdir.refresh();
  const output = spawnSync(process.execPath, [
    '--cpu-prof',
    '--cpu-prof-interval',
    kCpuProfInterval,
    fixtures.path('workload', 'fibonacci.js'),
  ], {
    cwd: tmpdir.path,
    env
  });
  if (output.status !== 0) {
    console.log(output.stderr.toString());
  }
  assert.strictEqual(output.status, 0);
  const profiles = getCpuProfiles(tmpdir.path);
  assert.strictEqual(profiles.length, 1);
  verifyFrames(output, profiles[0], 'fibonacci.js');
}

// Outputs CPU profile when process.exit(55) exits process.
{
  tmpdir.refresh();
  const output = spawnSync(process.execPath, [
    '--cpu-prof',
    '--cpu-prof-interval',
    kCpuProfInterval,
    fixtures.path('workload', 'fibonacci-exit.js'),
  ], {
    cwd: tmpdir.path,
    env
  });
  if (output.status !== 55) {
    console.log(output.stderr.toString());
  }
  assert.strictEqual(output.status, 55);
  const profiles = getCpuProfiles(tmpdir.path);
  assert.strictEqual(profiles.length, 1);
  verifyFrames(output, profiles[0], 'fibonacci-exit.js');
}

// Outputs CPU profile when process.kill(process.pid, "SIGINT"); exits process.
{
  tmpdir.refresh();
  const output = spawnSync(process.execPath, [
    '--cpu-prof',
    '--cpu-prof-interval',
    kCpuProfInterval,
    fixtures.path('workload', 'fibonacci-sigint.js'),
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
  const profiles = getCpuProfiles(tmpdir.path);
  assert.strictEqual(profiles.length, 1);
  verifyFrames(output, profiles[0], 'fibonacci-sigint.js');
}

// Outputs CPU profile from worker when execArgv is set.
{
  tmpdir.refresh();
  const output = spawnSync(process.execPath, [
    fixtures.path('workload', 'fibonacci-worker-argv.js'),
  ], {
    cwd: tmpdir.path,
    env: {
      ...process.env,
      CPU_PROF_INTERVAL: kCpuProfInterval
    }
  });
  if (output.status !== 0) {
    console.log(output.stderr.toString());
  }
  assert.strictEqual(output.status, 0);
  const profiles = getCpuProfiles(tmpdir.path);
  assert.strictEqual(profiles.length, 1);
  verifyFrames(output, profiles[0], 'fibonacci.js');
}

// --cpu-prof-name without --cpu-prof
{
  tmpdir.refresh();
  const output = spawnSync(process.execPath, [
    '--cpu-prof-name',
    'test.cpuprofile',
    fixtures.path('workload', 'fibonacci.js'),
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
    `${process.execPath}: --cpu-prof-name must be used with --cpu-prof`);
}

// --cpu-prof-dir without --cpu-prof
{
  tmpdir.refresh();
  const output = spawnSync(process.execPath, [
    '--cpu-prof-dir',
    'prof',
    fixtures.path('workload', 'fibonacci.js'),
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
    `${process.execPath}: --cpu-prof-dir must be used with --cpu-prof`);
}

// --cpu-prof-interval without --cpu-prof
{
  tmpdir.refresh();
  const output = spawnSync(process.execPath, [
    '--cpu-prof-interval',
    kCpuProfInterval,
    fixtures.path('workload', 'fibonacci.js'),
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
    `${process.execPath}: --cpu-prof-interval must be used with --cpu-prof`);
}

// --cpu-prof-name
{
  tmpdir.refresh();
  const file = path.join(tmpdir.path, 'test.cpuprofile');
  const output = spawnSync(process.execPath, [
    '--cpu-prof',
    '--cpu-prof-interval',
    kCpuProfInterval,
    '--cpu-prof-name',
    'test.cpuprofile',
    fixtures.path('workload', 'fibonacci.js'),
  ], {
    cwd: tmpdir.path,
    env
  });
  if (output.status !== 0) {
    console.log(output.stderr.toString());
  }
  assert.strictEqual(output.status, 0);
  const profiles = getCpuProfiles(tmpdir.path);
  assert.deepStrictEqual(profiles, [file]);
  verifyFrames(output, file, 'fibonacci.js');
}

// relative --cpu-prof-dir
{
  tmpdir.refresh();
  const output = spawnSync(process.execPath, [
    '--cpu-prof',
    '--cpu-prof-interval',
    kCpuProfInterval,
    '--cpu-prof-dir',
    'prof',
    fixtures.path('workload', 'fibonacci.js'),
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
  const profiles = getCpuProfiles(dir);
  assert.strictEqual(profiles.length, 1);
  verifyFrames(output, profiles[0], 'fibonacci.js');
}

// absolute --cpu-prof-dir
{
  tmpdir.refresh();
  const dir = path.join(tmpdir.path, 'prof');
  const output = spawnSync(process.execPath, [
    '--cpu-prof',
    '--cpu-prof-interval',
    kCpuProfInterval,
    '--cpu-prof-dir',
    dir,
    fixtures.path('workload', 'fibonacci.js'),
  ], {
    cwd: tmpdir.path,
    env
  });
  if (output.status !== 0) {
    console.log(output.stderr.toString());
  }
  assert.strictEqual(output.status, 0);
  assert(fs.existsSync(dir));
  const profiles = getCpuProfiles(dir);
  assert.strictEqual(profiles.length, 1);
  verifyFrames(output, profiles[0], 'fibonacci.js');
}

// --cpu-prof-dir and --cpu-prof-name
{
  tmpdir.refresh();
  const dir = path.join(tmpdir.path, 'prof');
  const file = path.join(dir, 'test.cpuprofile');
  const output = spawnSync(process.execPath, [
    '--cpu-prof',
    '--cpu-prof-interval',
    kCpuProfInterval,
    '--cpu-prof-name',
    'test.cpuprofile',
    '--cpu-prof-dir',
    dir,
    fixtures.path('workload', 'fibonacci.js'),
  ], {
    cwd: tmpdir.path,
    env
  });
  if (output.status !== 0) {
    console.log(output.stderr.toString());
  }
  assert.strictEqual(output.status, 0);
  assert(fs.existsSync(dir));
  const profiles = getCpuProfiles(dir);
  assert.deepStrictEqual(profiles, [file]);
  verifyFrames(output, file, 'fibonacci.js');
}

// --cpu-prof-dir with worker
{
  tmpdir.refresh();
  const output = spawnSync(process.execPath, [
    '--cpu-prof-interval',
    kCpuProfInterval,
    '--cpu-prof-dir',
    'prof',
    '--cpu-prof',
    fixtures.path('workload', 'fibonacci-worker.js'),
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
  const profiles = getCpuProfiles(dir);
  assert.strictEqual(profiles.length, 2);
  const profile1 = getFrames(output, profiles[0], 'fibonacci.js');
  const profile2 = getFrames(output, profiles[1], 'fibonacci.js');
  if (profile1.frames.length === 0 && profile2.frames.length === 0) {
    // Show native debug output and the profile for debugging.
    console.log(output.stderr.toString());
    console.log('CPU path: ', profiles[0]);
    console.log(profile1.nodes);
    console.log('CPU path: ', profiles[1]);
    console.log(profile2.nodes);
  }
  assert(profile1.frames.length > 0 || profile2.frames.length > 0);
}
