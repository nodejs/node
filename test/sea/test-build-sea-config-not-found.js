// This tests --build-sea when the config file doesn't exist.

'use strict';

require('../common');
const tmpdir = require('../common/tmpdir');
const { skipIfBuildSEAIsNotSupported } = require('../common/sea');
const { spawnSyncAndAssert } = require('../common/child_process');

skipIfBuildSEAIsNotSupported();

// Test: Config file doesn't exist (relative path)
{
  tmpdir.refresh();
  const config = 'non-existent-relative.json';
  spawnSyncAndAssert(
    process.execPath,
    ['--build-sea', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /Cannot read single executable configuration from non-existent-relative\.json/,
    });
}

// Test: Config file doesn't exist (absolute path)
{
  tmpdir.refresh();
  const config = tmpdir.resolve('non-existent-absolute.json');
  spawnSyncAndAssert(
    process.execPath,
    ['--build-sea', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /Cannot read single executable configuration from .*non-existent-absolute\.json/,
    });
}
