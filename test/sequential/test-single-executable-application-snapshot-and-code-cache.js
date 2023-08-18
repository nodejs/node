'use strict';

require('../common');

const {
  injectAndCodeSign,
  skipIfSingleExecutableIsNotSupported,
} = require('../common/sea');

skipIfSingleExecutableIsNotSupported();

// This tests "useCodeCache" is ignored when "useSnapshot" is true.

const tmpdir = require('../common/tmpdir');
const { copyFileSync, writeFileSync, existsSync } = require('fs');
const { spawnSync } = require('child_process');
const { join } = require('path');
const assert = require('assert');

const configFile = join(tmpdir.path, 'sea-config.json');
const seaPrepBlob = join(tmpdir.path, 'sea-prep.blob');
const outputFile = join(tmpdir.path, process.platform === 'win32' ? 'sea.exe' : 'sea');

{
  tmpdir.refresh();
  const code = `
  const {
    setDeserializeMainFunction,
  } = require('v8').startupSnapshot;

  setDeserializeMainFunction(() => {
    console.log('Hello from snapshot');
  });
  `;

  writeFileSync(join(tmpdir.path, 'snapshot.js'), code, 'utf-8');
  writeFileSync(configFile, `
  {
    "main": "snapshot.js",
    "output": "sea-prep.blob",
    "useSnapshot": true,
    "useCodeCache": true
  }
  `);

  let child = spawnSync(
    process.execPath,
    ['--experimental-sea-config', 'sea-config.json'],
    {
      cwd: tmpdir.path
    });
  assert.match(
    child.stderr.toString(),
    /"useCodeCache" is redundant when "useSnapshot" is true/);

  assert(existsSync(seaPrepBlob));

  copyFileSync(process.execPath, outputFile);
  injectAndCodeSign(outputFile, seaPrepBlob);

  child = spawnSync(outputFile);
  assert.strictEqual(child.stdout.toString().trim(), 'Hello from snapshot');
}
