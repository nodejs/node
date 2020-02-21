'use strict';
const common = require('../common');

// Test creating a symbolic link pointing to a directory.
// Ref: https://github.com/nodejs/node/pull/23724
// Ref: https://github.com/nodejs/node/issues/23596


if (!common.canCreateSymLink())
  common.skip('insufficient privileges');

const assert = require('assert');
const path = require('path');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const linkTargets = [
  'relative-target',
  path.join(tmpdir.path, 'absolute-target')
];
const linkPaths = [
  path.relative(process.cwd(), path.join(tmpdir.path, 'relative-path')),
  path.join(tmpdir.path, 'absolute-path')
];

function testSync(target, path) {
  fs.symlinkSync(target, path);
  fs.readdirSync(path);
}

function testAsync(target, path) {
  fs.symlink(target, path, common.mustCall((err) => {
    assert.ifError(err);
    fs.readdirSync(path);
  }));
}

for (const linkTarget of linkTargets) {
  fs.mkdirSync(path.resolve(tmpdir.path, linkTarget));
  for (const linkPath of linkPaths) {
    testSync(linkTarget, `${linkPath}-${path.basename(linkTarget)}-sync`);
    testAsync(linkTarget, `${linkPath}-${path.basename(linkTarget)}-async`);
  }
}

// Test invalid symlink
{
  function testSync(target, path) {
    fs.symlinkSync(target, path);
    assert(!fs.existsSync(path));
  }

  function testAsync(target, path) {
    fs.symlink(target, path, common.mustCall((err) => {
      assert.ifError(err);
      assert(!fs.existsSync(path));
    }));
  }

  for (const linkTarget of linkTargets.map((p) => p + '-broken')) {
    for (const linkPath of linkPaths) {
      testSync(linkTarget, `${linkPath}-${path.basename(linkTarget)}-sync`);
      testAsync(linkTarget, `${linkPath}-${path.basename(linkTarget)}-async`);
    }
  }
}
