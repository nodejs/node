'use strict';

// This tests that --cpu-prof and --cpu-prof-path works.

const common = require('../common');
if (process.features.debug &&
  process.config.variables.node_code_cache_path === 'yes') {
  // FIXME(joyeecheung): the profiler crashes when code cache
  // is enabled in debug builds.
  common.skip('--cpu-prof does not work in debug builds with code cache');
}

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

function verifyFrames(output, file, suffix) {
  const data = fs.readFileSync(file, 'utf8');
  const profile = JSON.parse(data);
  const frames = profile.nodes.filter((i) => {
    const frame = i.callFrame;
    return frame.url.endsWith(suffix);
  });
  if (frames.length === 0) {
    // Show native debug output and the profile for debugging.
    console.log(output.stderr.toString());
    console.log(profile.nodes);
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

const env = {
  ...process.env,
  FIB,
  NODE_DEBUG_NATIVE: 'INSPECTOR_PROFILER'
};

// Outputs CPU profile when event loop is drained.
// TODO(joyeecheung): share the fixutres with v8 coverage tests
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
  verifyFrames(output, profiles[0], 'fibonacci.js');
}

// Outputs CPU profile when process.exit(55) exits process.
{
  tmpdir.refresh();
  const output = spawnSync(process.execPath, [
    '--cpu-prof',
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

// Outputs CPU profile from worker.
{
  tmpdir.refresh();
  const output = spawnSync(process.execPath, [
    fixtures.path('workload', 'fibonacci-worker.js'),
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

// Output to specified --cpu-prof-path without --cpu-prof
{
  tmpdir.refresh();
  const file = path.join(tmpdir.path, 'test.cpuprofile');
  const output = spawnSync(process.execPath, [
    '--cpu-prof-path',
    file,
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

// Output to specified --cpu-prof-path with --cpu-prof
{
  tmpdir.refresh();
  const file = path.join(tmpdir.path, 'test.cpuprofile');
  const output = spawnSync(process.execPath, [
    '--cpu-prof',
    '--cpu-prof-path',
    file,
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

// Output to specified --cpu-prof-path when it's not absolute
{
  tmpdir.refresh();
  const file = path.join(tmpdir.path, 'test.cpuprofile');
  const output = spawnSync(process.execPath, [
    '--cpu-prof',
    '--cpu-prof-path',
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
