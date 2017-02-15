'use strict';
const common = require('../common');
if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

const path = require('path');
const spawn = require('child_process').spawn;
const assert = require('assert');
const fs = require('fs');

common.refreshTmpDir();
const npmSandbox = path.join(common.tmpDir, 'npm-sandbox');
fs.mkdirSync(npmSandbox);
const installDir = path.join(common.tmpDir, 'install-dir');
fs.mkdirSync(installDir);

const npmPath = path.join(
  __dirname,
  '..',
  '..',
  'deps',
  'npm',
  'bin',
  'npm-cli.js'
);

const args = [
  npmPath,
  'install'
];

const pkgContent = JSON.stringify({
  dependencies: {
    'package-name': common.fixturesDir + '/packages/main'
  }
});

const pkgPath = path.join(installDir, 'package.json');

fs.writeFileSync(pkgPath, pkgContent);

const env = Object.create(process.env);
env['PATH'] = path.dirname(process.execPath);
env['NPM_CONFIG_PREFIX'] = path.join(npmSandbox, 'npm-prefix');
env['NPM_CONFIG_TMP'] = path.join(npmSandbox, 'npm-tmp');
env['HOME'] = path.join(npmSandbox, 'home');

const proc = spawn(process.execPath, args, {
  cwd: installDir,
  env: env
});

function handleExit(code, signalCode) {
  assert.strictEqual(code, 0, `npm install got error code ${code}`);
  assert.strictEqual(signalCode, null, `unexpected signal: ${signalCode}`);
  assert.doesNotThrow(function() {
    fs.accessSync(installDir + '/node_modules/package-name');
  });
}

proc.on('exit', common.mustCall(handleExit));
