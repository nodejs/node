'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
if (common.isInsideDirWithUnusualChars)
  common.skip('npm does not support this install path');

const path = require('path');
const assert = require('assert');
const fs = require('fs');
const fixtures = require('../common/fixtures');
const { spawnSyncAndAssert } = require('../common/child_process');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

// Copy fixtures/npm-install to the tmpdir for testing
fs.cpSync(fixtures.path('npm-install'), tmpdir.path, { recursive: true });

const npmPath = path.join(__dirname, '..', '..', 'deps', 'npm', 'bin', 'npm-cli.js');

const env = {
  ...process.env,
  PATH: path.dirname(process.execPath),
  NODE: process.execPath,
  NPM: npmPath,
  NPM_CONFIG_PREFIX: tmpdir.resolve('npm-prefix'),
  NPM_CONFIG_TMP: tmpdir.resolve('npm-tmp'),
  NPM_CONFIG_AUDIT: false,
  NPM_CONFIG_UPDATE_NOTIFIER: false,
  HOME: tmpdir.resolve('home'),
};

const installDir = tmpdir.resolve('install-dir');
spawnSyncAndAssert(
  process.execPath,
  [npmPath, 'install'],
  { cwd: installDir, env },
  {}
);

assert(fs.existsSync(path.join(installDir, 'node_modules', 'example')));
