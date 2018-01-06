// Flags: --preserve-symlinks
'use strict';
const common = require('../common');

if (!common.canCreateSymLink())
  common.skip('insufficient privileges');

const assert = require('assert');
const { spawn } = require('child_process');
const fs = require('fs');
const path = require('path');
const process = require('process');

// Setup: Copy fixtures to tmp directory.

const fixtures = require('../common/fixtures');
const dirName = 'module-require-symlink';
const fixtureSource = fixtures.path(dirName);
const tmpDirTarget = path.join(common.tmpDir, dirName);

// Copy fixtureSource to linkTarget recursively.
common.refreshTmpDir();

function copyDir(source, target) {
  fs.mkdirSync(target);
  fs.readdirSync(source).forEach((entry) => {
    const fullPathSource = path.join(source, entry);
    const fullPathTarget = path.join(target, entry);
    const stats = fs.statSync(fullPathSource);
    if (stats.isDirectory()) {
      copyDir(fullPathSource, fullPathTarget);
    } else {
      fs.copyFileSync(fullPathSource, fullPathTarget);
    }
  });
}

copyDir(fixtureSource, tmpDirTarget);

// Move to tmp dir and do everything with relative paths there so that the test
// doesn't incorrectly fail due to a symlink somewhere else in the absolute
// path.
process.chdir(common.tmpDir);

const linkDir = path.join(dirName,
                          'node_modules',
                          'dep1',
                          'node_modules',
                          'dep2');

const linkTarget = path.join('..', '..', 'dep2');

const linkScript = 'linkscript.js';

const linkScriptTarget = path.join(dirName, 'symlinked.js');

test();

function test() {
  fs.symlinkSync(linkTarget, linkDir);
  fs.symlinkSync(linkScriptTarget, linkScript);

  // load symlinked-module
  const fooModule = require(path.join(tmpDirTarget, 'foo.js'));
  assert.strictEqual(fooModule.dep1.bar.version, 'CORRECT_VERSION');
  assert.strictEqual(fooModule.dep2.bar.version, 'CORRECT_VERSION');

  // load symlinked-script as main
  const node = process.execPath;
  const child = spawn(node, ['--preserve-symlinks', linkScript]);
  child.on('close', function(code, signal) {
    assert.strictEqual(code, 0);
    assert(!signal);
  });

  // Also verify that symlinks works for setting preserve via env variables
  const childEnv = spawn(node, [linkScript], {
    env: Object.assign({}, process.env, { NODE_PRESERVE_SYMLINKS: '1' })
  });
  childEnv.on('close', function(code, signal) {
    assert.strictEqual(code, 0);
    assert(!signal);
  });
}
