'use strict';

// This tests that --cpu-prof-name can be used to specify the
// name of the generated CPU profile.

const common = require('../common');
const fixtures = require('../common/fixtures');
common.skipIfInspectorDisabled();

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');

const tmpdir = require('../common/tmpdir');
const {
  getCpuProfiles,
  kCpuProfInterval,
  env,
  verifyFrames
} = require('../common/cpu-prof');

// --cpu-prof-name
{
  tmpdir.refresh();
  const file = tmpdir.resolve('test.cpuprofile');
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

// --cpu-prof-name with ${pid} placeholder
{
  tmpdir.refresh();
  // eslint-disable-next-line no-template-curly-in-string
  const profName = 'CPU.${pid}.cpuprofile';
  const dir = tmpdir.path;

  const output = spawnSync(process.execPath, [
    '--cpu-prof',
    '--cpu-prof-interval',
    kCpuProfInterval,
    '--cpu-prof-name',
    profName,
    fixtures.path('workload', 'fibonacci.js'),
  ], {
    cwd: dir,
    env
  });

  if (output.status !== 0) {
    console.error(output.stderr.toString());
  }

  assert.strictEqual(output.status, 0);

  const expectedFile = path.join(dir, `CPU.${output.pid}.cpuprofile`);
  assert.ok(fs.existsSync(expectedFile), `Expected file ${expectedFile} not found.`);

  verifyFrames(output, expectedFile, 'fibonacci.js');

  fs.unlinkSync(expectedFile);
}
