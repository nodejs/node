'use strict';

require('../common');

const {
  generateSEA,
  skipIfSingleExecutableIsNotSupported,
} = require('../common/sea');

skipIfSingleExecutableIsNotSupported();

// This tests the execArgvExtension "env" mode (default) in single executable applications.

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
copyFileSync(fixtures.path('sea-exec-argv-extension-env.js'), tmpdir.resolve('sea.js'));

writeFileSync(configFile, `
{
  "main": "sea.js",
  "output": "sea-prep.blob",
  "disableExperimentalSEAWarning": true,
  "execArgv": ["--no-warnings"],
  "execArgvExtension": "env"
}
`);

spawnSyncAndExitWithoutError(
  process.execPath,
  ['--experimental-sea-config', 'sea-config.json'],
  { cwd: tmpdir.path });

assert(existsSync(seaPrepBlob));

generateSEA(outputFile, process.execPath, seaPrepBlob);

// Test that NODE_OPTIONS works with execArgvExtension: "env" (default behavior)
spawnSyncAndAssert(
  outputFile,
  ['user-arg1', 'user-arg2'],
  {
    env: {
      ...process.env,
      NODE_OPTIONS: '--max-old-space-size=512',
      COMMON_DIRECTORY: join(__dirname, '..', 'common'),
      NODE_DEBUG_NATIVE: 'SEA',
    }
  },
  {
    stdout: /execArgvExtension env test passed/,
    stderr(output) {
      assert.doesNotMatch(output, /This warning should not be shown in the output/);
      return true;
    },
    trim: true
  });
