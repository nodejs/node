// Flags: --preserve-symlinks
'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const exec = require('child_process').exec;
const spawn = require('child_process').spawn;

const linkTarget = path.join(common.fixturesDir,
  '/module-require-symlink/node_modules/dep2/');

const linkDir = path.join(common.fixturesDir,
  '/module-require-symlink/node_modules/dep1/node_modules/dep2');

const linkScriptTarget = path.join(common.fixturesDir,
  '/module-require-symlink/symlinked.js');

const linkScript = path.join(common.tmpDir, 'module-require-symlink.js');

if (common.isWindows) {
  // On Windows, creating symlinks requires admin privileges.
  // We'll only try to run symlink test if we have enough privileges.
  exec('whoami /priv', function(err, o) {
    if (err || o.indexOf('SeCreateSymbolicLinkPrivilege') == -1) {
      console.log('Skipped: insufficient privileges');
      return;
    } else {
      test();
    }
  });
} else {
  test();
}

function test() {
  process.on('exit', function() {
    fs.unlinkSync(linkDir);
    fs.unlinkSync(linkScript);
  });

  fs.symlinkSync(linkTarget, linkDir);
  fs.symlinkSync(linkScriptTarget, linkScript);

  // load symlinked-module
  var fooModule =
    require(path.join(common.fixturesDir, '/module-require-symlink/foo.js'));
  assert.equal(fooModule.dep1.bar.version, 'CORRECT_VERSION');
  assert.equal(fooModule.dep2.bar.version, 'CORRECT_VERSION');

  // load symlinked-script as main
  var node = process.execPath;
  var child = spawn(node, ['--preserve-symlinks', linkScript]);
  child.on('close', function(code, signal) {
    assert(!code);
    assert(!signal);
  });
}
