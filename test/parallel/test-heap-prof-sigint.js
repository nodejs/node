'use strict';

// Tests --heap-prof generates a heap profile when
// process.kill(process.pid, "SIGINT"); exits process.

const common = require('../common');

const fixtures = require('../common/fixtures');
common.skipIfInspectorDisabled();

const assert = require('assert');
const { spawnSync } = require('child_process');

const tmpdir = require('../common/tmpdir');

const {
  getHeapProfiles,
  verifyFrames,
  kHeapProfInterval,
  env
} = require('../common/prof');

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
