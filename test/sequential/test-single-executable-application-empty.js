'use strict';

require('../common');

const {
  injectAndCodeSign,
  skipIfSingleExecutableIsNotSupported,
} = require('../common/sea');

skipIfSingleExecutableIsNotSupported();

// This tests the creation of a single executable application with an empty
// script.

const tmpdir = require('../common/tmpdir');
const { copyFileSync, writeFileSync, existsSync } = require('fs');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');
const assert = require('assert');

const configFile = tmpdir.resolve('sea-config.json');
const seaPrepBlob = tmpdir.resolve('sea-prep.blob');
const outputFile = tmpdir.resolve(process.platform === 'win32' ? 'sea.exe' : 'sea');

tmpdir.refresh();

writeFileSync(tmpdir.resolve('empty.js'), '', 'utf-8');
writeFileSync(configFile, `
{
  "main": "empty.js",
  "output": "sea-prep.blob"
}
`);

spawnSyncAndExitWithoutError(
  process.execPath,
  ['--experimental-sea-config', 'sea-config.json'],
  { cwd: tmpdir.path });

assert(existsSync(seaPrepBlob));

copyFileSync(process.execPath, outputFile);
injectAndCodeSign(outputFile, seaPrepBlob);

spawnSyncAndExitWithoutError(
  outputFile,
  {
    env: {
      NODE_DEBUG_NATIVE: 'SEA',
      ...process.env,
    }
  },
  {});
