'use strict';

// This tests that --cpu-prof generates CPU profile when
// process.kill(process.pid, "SIGINT"); exits the process.

const common = require('../common');
const fixtures = require('../common/fixtures');
common.skipIfInspectorDisabled();

const assert = require('assert');
const { spawnSync } = require('child_process');

const tmpdir = require('../common/tmpdir');
const {
  getCpuProfiles,
  kCpuProfInterval,
  env,
  verifyFrames
} = require('../common/cpu-prof');

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
