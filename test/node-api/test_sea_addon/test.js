'use strict';
// This tests that SEA can load addons packaged as assets by writing them to disk
// and loading them via process.dlopen().
const common = require('../../common');
const { generateSEA, skipIfSingleExecutableIsNotSupported } = require('../../common/sea');

skipIfSingleExecutableIsNotSupported();

const assert = require('assert');

const tmpdir = require('../../common/tmpdir');
const { copyFileSync, writeFileSync, existsSync, rmSync } = require('fs');
const {
  spawnSyncAndExitWithoutError,
  spawnSyncAndAssert,
} = require('../../common/child_process');
const { join } = require('path');
const configFile = tmpdir.resolve('sea-config.json');
const seaPrepBlob = tmpdir.resolve('sea-prep.blob');
const outputFile = tmpdir.resolve(process.platform === 'win32' ? 'sea.exe' : 'sea');
tmpdir.refresh();

// Copy test fixture to working directory
const addonPath = join(__dirname, 'build', common.buildType, 'binding.node');
const copiedAddonPath = tmpdir.resolve('binding.node');
copyFileSync(addonPath, copiedAddonPath);
writeFileSync(tmpdir.resolve('sea.js'), `
const sea = require('node:sea');
const fs = require('fs');
const path = require('path');

const addonPath = path.join(${JSON.stringify(tmpdir.path)}, 'hello.node');
fs.writeFileSync(addonPath, new Uint8Array(sea.getRawAsset('hello.node')));
const mod = {exports: {}}
process.dlopen(mod, addonPath);
console.log('hello,', mod.exports.hello());
`, 'utf-8');

writeFileSync(configFile, `
{
  "main": "sea.js",
  "output": "sea-prep.blob",
  "disableExperimentalSEAWarning": true,
  "assets": {
    "hello.node": "binding.node"
  }
}
`, 'utf8');

spawnSyncAndExitWithoutError(
  process.execPath,
  ['--experimental-sea-config', 'sea-config.json'],
  { cwd: tmpdir.path },
);
assert(existsSync(seaPrepBlob));

generateSEA(outputFile, process.execPath, seaPrepBlob);

// Remove the copied addon after it's been packaged into the SEA blob
rmSync(copiedAddonPath, { force: true });

spawnSyncAndAssert(
  outputFile,
  [],
  {
    env: {
      ...process.env,
      NODE_DEBUG_NATIVE: 'SEA',
    },
    cwd: tmpdir.path,
  },
  {
    stdout: /hello, world/,
  },
);
