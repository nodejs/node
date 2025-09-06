'use strict';

require('../common');

const {
  generateSEA,
  skipIfSingleExecutableIsNotSupported,
} = require('../common/sea');

skipIfSingleExecutableIsNotSupported();

// This tests the argv functionality with multiple arguments in single executable applications.

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
copyFileSync(fixtures.path('sea-argv.js'), tmpdir.resolve('sea.js'));

writeFileSync(configFile, `
{
  "main": "sea.js",
  "output": "sea-prep.blob",
  "disableExperimentalSEAWarning": true,
  "argv": ["user-arg1", "user-arg2"]
}
`);

spawnSyncAndExitWithoutError(
  process.execPath,
  ['--experimental-sea-config', 'sea-config.json'],
  { cwd: tmpdir.path });

assert(existsSync(seaPrepBlob));

generateSEA(outputFile, process.execPath, seaPrepBlob);

// Test that multiple argv are properly applied
spawnSyncAndAssert(
  outputFile,
  [
    'user-arg3',
  ],
  {
    env: {
      ...process.env,
      NODE_NO_WARNINGS: '0',
      COMMON_DIRECTORY: join(__dirname, '..', 'common'),
      NODE_DEBUG_NATIVE: 'SEA',
    }
  },
  {
    stdout: /multiple argv test passed/,
    trim: true,
  });
