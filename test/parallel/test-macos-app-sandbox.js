'use strict';
const common = require('../common');
if (process.platform !== 'darwin')
  common.skip('App Sandbox is only available on Darwin');

const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const child_process = require('child_process');
const path = require('path');
const fs = require('fs');
const os = require('os');

const nodeBinary = process.execPath;

tmpdir.refresh();

const appBundlePath = path.join(tmpdir.path, 'node_sandboxed.app');
const appBundleContentPath = path.join(appBundlePath, 'Contents');
const appExecutablePath = path.join(
  appBundleContentPath, 'MacOS', 'node');

// Construct the app bundle and put the node executable in it:
// node_sandboxed.app/
// └── Contents
//     ├── Info.plist
//     ├── MacOS
//     │   └── node
fs.mkdirSync(appBundlePath);
fs.mkdirSync(appBundleContentPath);
fs.mkdirSync(path.join(appBundleContentPath, 'MacOS'));
fs.copyFileSync(
  fixtures.path('macos-app-sandbox', 'Info.plist'),
  path.join(appBundleContentPath, 'Info.plist'));
fs.copyFileSync(
  nodeBinary,
  appExecutablePath);


// Sign the app bundle with sandbox entitlements:
assert.strictEqual(
  child_process.spawnSync('/usr/bin/codesign', [
    '--entitlements', fixtures.path(
      'macos-app-sandbox', 'node_sandboxed.entitlements'),
    '--force', '-s', '-',
    appBundlePath,
  ]).status,
  0);

// Sandboxed app shouldn't be able to read the home dir
assert.notStrictEqual(
  child_process.spawnSync(appExecutablePath, [
    '-e', 'fs.readdirSync(process.argv[1])', os.homedir(),
  ]).status,
  0);

if (process.stdin.isTTY) {
  // Run the sandboxed node instance with inherited tty stdin
  const spawnResult = child_process.spawnSync(
    appExecutablePath, ['-e', ''],
    { stdio: 'inherit' }
  );

  assert.strictEqual(spawnResult.signal, null);
}
