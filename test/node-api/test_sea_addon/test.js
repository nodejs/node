'use strict';
// This tests that SEA can load addons packaged as assets by writing them to disk
// and loading them via process.dlopen().
const common = require('../../common');
const { generateSEA } = require('../../common/sea');
const assert = require('assert');

const tmpdir = require('../../common/tmpdir');
const { copyFileSync, writeFileSync, existsSync } = require('fs');
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
copyFileSync(addonPath, tmpdir.resolve('binding.node'));
writeFileSync(tmpdir.resolve('sea.js'), `
const sea = require('node:sea');
const fs = require('fs');
const os = require('os');
const path = require('path');

const dir = fs.mkdtempSync(os.tmpdir());
const addonPath = path.join(dir, 'hello.node');
fs.writeFileSync(addonPath, new Uint8Array(sea.getRawAsset('hello.node')));
const mod = {exports: {}}
process.dlopen(mod, addonPath);
console.log('hello,', mod.exports.hello());
fs.rmSync(dir, { recursive: true });
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
spawnSyncAndAssert(
  outputFile,
  [],
  {
    env: {
      ...process.env,
      NODE_DEBUG_NATIVE: 'SEA',
    },
  },
  {
    stdout: /hello, world/,
  },
);
