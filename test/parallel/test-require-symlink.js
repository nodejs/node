// Flags: --preserve-symlinks
'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const { exec, spawn } = require('child_process');
const util = require('util');
const fixtures = require('../common/fixtures');

common.refreshTmpDir();

const linkTarget = fixtures.path('module-require-symlink',
                                 'node_modules',
                                 'dep2');

const linkDir = fixtures.path('module-require-symlink',
                              'node_modules',
                              'dep1',
                              'node_modules',
                              'dep2');

const linkScriptTarget = fixtures.path('module-require-symlink',
                                       'symlinked.js');

const linkScript = path.join(common.tmpDir, 'module-require-symlink.js');

if (common.isWindows) {
  // On Windows, creating symlinks requires admin privileges.
  // We'll only try to run symlink test if we have enough privileges.
  exec('whoami /priv', function(err, o) {
    if (err || !o.includes('SeCreateSymbolicLinkPrivilege'))
      common.skip('insufficient privileges');

    test();
  });
} else {
  test();
}

function test() {
  process.on('exit', function() {
    fs.unlinkSync(linkDir);
  });

  fs.symlinkSync(linkTarget, linkDir);
  fs.symlinkSync(linkScriptTarget, linkScript);

  // load symlinked-module
  const fooModule = require(fixtures.path('/module-require-symlink/foo.js'));
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
    env: util._extend(process.env, { NODE_PRESERVE_SYMLINKS: '1' })
  });
  childEnv.on('close', function(code, signal) {
    assert.strictEqual(code, 0);
    assert(!signal);
  });
}
