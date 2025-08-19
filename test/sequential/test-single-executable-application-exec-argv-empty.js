'use strict';

require('../common');

const {
  generateSEA,
  skipIfSingleExecutableIsNotSupported,
} = require('../common/sea');

skipIfSingleExecutableIsNotSupported();

// This tests the execArgv functionality with empty array in single executable applications.

const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const { copyFileSync, writeFileSync, existsSync } = require('fs');
const { spawnSyncAndAssert, spawnSyncAndExitWithoutError } = require('../common/child_process');
const { join } = require('path');
const assert = require('assert');

const configFile = tmpdir.resolve('sea-config.json');
const seaPrepBlob = tmpdir.resolve('sea-prep.blob');
const outputFile = tmpdir.resolve(process.platform === 'win32' ? 'sea.exe' : 'sea');

tmpdir.refresh();

// Copy test fixture to working directory
copyFileSync(fixtures.path('sea-exec-argv-empty.js'), tmpdir.resolve('sea.js'));

writeFileSync(configFile, `
{
  "main": "sea.js",
  "output": "sea-prep.blob",
  "disableExperimentalSEAWarning": true,
  "execArgv": []
}
`);

spawnSyncAndExitWithoutError(
  process.execPath,
  ['--experimental-sea-config', 'sea-config.json'],
  { cwd: tmpdir.path });

assert(existsSync(seaPrepBlob));

generateSEA(outputFile, process.execPath, seaPrepBlob);

// Test that empty execArgv work correctly
spawnSyncAndAssert(
  outputFile,
  ['user-arg'],
  {
    env: {
      COMMON_DIRECTORY: join(__dirname, '..', 'common'),
      NODE_DEBUG_NATIVE: 'SEA',
      ...process.env,
    }
  },
  {
    stdout: /empty execArgv test passed/
  });
