'use strict';

// This tests that the debugger flag --inspect passed directly to a single executable
// application would not be consumed by Node.js but passed to the application
// instead.

require('../common');
const assert = require('assert');
const { writeFileSync, existsSync } = require('fs');
const { spawnSyncAndAssert } = require('../common/child_process');
const tmpdir = require('../common/tmpdir');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');

const {
  generateSEA,
  skipIfSingleExecutableIsNotSupported,
} = require('../common/sea');

skipIfSingleExecutableIsNotSupported();

const configFile = tmpdir.resolve('sea-config.json');
const seaPrepBlob = tmpdir.resolve('sea-prep.blob');
const outputFile = tmpdir.resolve(process.platform === 'win32' ? 'sea.exe' : 'sea');

tmpdir.refresh();

writeFileSync(tmpdir.resolve('sea.js'), `console.log(process.argv);`, 'utf-8');

// Create SEA configuration
writeFileSync(configFile, `
{
  "main": "sea.js",
  "output": "sea-prep.blob"
}
`);

// Generate the SEA prep blob
spawnSyncAndExitWithoutError(
  process.execPath,
  ['--experimental-sea-config', 'sea-config.json'],
  { cwd: tmpdir.path }
);

assert(existsSync(seaPrepBlob));

// Generate the SEA executable
generateSEA(outputFile, process.execPath, seaPrepBlob, true);

// Spawn the SEA with inspect option
spawnSyncAndAssert(
  outputFile,
  ['--inspect=0'],
  {
    env: {
      ...process.env,
    },
  },
  {
    stdout(data) {
      assert.match(data, /--inspect=0/);
      return true;
    },
    stderr(data) {
      assert.doesNotMatch(data, /Debugger listening/);
      return true;
    },
    trim: true,
  }
);
