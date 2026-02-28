// This tests --build-sea when boolean fields have invalid types.

'use strict';

require('../common');
const tmpdir = require('../common/tmpdir');
const { skipIfBuildSEAIsNotSupported } = require('../common/sea');
const { writeFileSync } = require('fs');
const { spawnSyncAndAssert } = require('../common/child_process');

skipIfBuildSEAIsNotSupported();

// Test: Invalid "disableExperimentalSEAWarning" type (should be Boolean)
{
  tmpdir.refresh();
  const config = tmpdir.resolve('invalid-disableExperimentalSEAWarning.json');
  writeFileSync(config, `
{
  "main": "bundle.js",
  "output": "sea",
  "disableExperimentalSEAWarning": "true"
}
  `, 'utf8');
  spawnSyncAndAssert(
    process.execPath,
    ['--build-sea', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /"disableExperimentalSEAWarning" field of .*invalid-disableExperimentalSEAWarning\.json is not a Boolean/,
    });
}

// Test: Invalid "useSnapshot" type (should be Boolean)
{
  tmpdir.refresh();
  const config = tmpdir.resolve('invalid-useSnapshot.json');
  writeFileSync(config, `
{
  "main": "bundle.js",
  "output": "sea",
  "useSnapshot": "false"
}
  `, 'utf8');
  spawnSyncAndAssert(
    process.execPath,
    ['--build-sea', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /"useSnapshot" field of .*invalid-useSnapshot\.json is not a Boolean/,
    });
}

// Test: Invalid "useCodeCache" type (should be Boolean)
{
  tmpdir.refresh();
  const config = tmpdir.resolve('invalid-useCodeCache.json');
  writeFileSync(config, `
{
  "main": "bundle.js",
  "output": "sea",
  "useCodeCache": 1
}
  `, 'utf8');
  spawnSyncAndAssert(
    process.execPath,
    ['--build-sea', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /"useCodeCache" field of .*invalid-useCodeCache\.json is not a Boolean/,
    });
}
