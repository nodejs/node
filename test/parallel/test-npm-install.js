'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const path = require('path');
const exec = require('child_process').exec;
const assert = require('assert');
const fs = require('fs');
const fixtures = require('../common/fixtures');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();
const npmSandbox = path.join(tmpdir.path, 'npm-sandbox');
fs.mkdirSync(npmSandbox);
const homeDir = path.join(tmpdir.path, 'home');
fs.mkdirSync(homeDir);
const installDir = path.join(tmpdir.path, 'install-dir');
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

const pkgContent = JSON.stringify({
  dependencies: {
    'package-name': fixtures.path('packages/main')
  }
});

const pkgPath = path.join(installDir, 'package.json');

fs.writeFileSync(pkgPath, pkgContent);

const env = { ...process.env,
              PATH: path.dirname(process.execPath),
              NPM_CONFIG_PREFIX: path.join(npmSandbox, 'npm-prefix'),
              NPM_CONFIG_TMP: path.join(npmSandbox, 'npm-tmp'),
              HOME: homeDir,
};

exec(`${process.execPath} ${npmPath} install`, {
  cwd: installDir,
  env: env
}, common.mustCall(handleExit));

function handleExit(error, stdout, stderr) {
  const code = error ? error.code : 0;
  const signalCode = error ? error.signal : null;

  if (code !== 0) {
    process.stderr.write(stderr);
  }

  assert.strictEqual(code, 0, `npm install got error code ${code}`);
  assert.strictEqual(signalCode, null, `unexpected signal: ${signalCode}`);
  assert(fs.existsSync(`${installDir}/node_modules/package-name`));
}
