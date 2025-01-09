'use strict';
const common = require('../common');
if (process.platform !== 'darwin')
  common.skip('App Sandbox is only available on Darwin');
if (process.config.variables.node_builtin_modules_path)
  common.skip('App Sandbox cannot load modules from outside the sandbox');

const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const path = require('path');
const fs = require('fs');
const os = require('os');
const {
  spawnSyncAndExit,
  spawnSyncAndExitWithoutError
} = require('../common/child_process');

const nodeBinary = process.execPath;

tmpdir.refresh();

if (!tmpdir.hasEnoughSpace(120 * 1024 * 1024)) {
  common.skip('Available disk space < 120MB');
}

const appBundlePath = tmpdir.resolve('node_sandboxed.app');
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
spawnSyncAndExitWithoutError('codesign', [
  '--entitlements',
  fixtures.path('macos-app-sandbox', 'node_sandboxed.entitlements'),
  '--force',
  '-s',
  '-',
  appBundlePath,
]);

// Sandboxed app shouldn't be able to read the home dir
spawnSyncAndExit(
  appExecutablePath,
  ['-e', 'fs.readdirSync(process.argv[1])', os.homedir()],
  {
    status: 1,
    signal: null,
  },
);

if (process.stdin.isTTY) {
  // Run the sandboxed node instance with inherited tty stdin
  spawnSyncAndExitWithoutError(appExecutablePath, ['-e', ''], {
    stdio: 'inherit',
  });
}
