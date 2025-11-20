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
const fsPromises = fs.promises;

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const linkTargets = [
  'relative-target',
  tmpdir.resolve('absolute-target'),
];
const linkPaths = [
  path.relative(process.cwd(), tmpdir.resolve('relative-path')),
  tmpdir.resolve('absolute-path'),
];

function testSync(target, path) {
  fs.symlinkSync(target, path);
  fs.readdirSync(path);
}

function testAsync(target, path) {
  fs.symlink(target, path, common.mustSucceed(() => {
    fs.readdirSync(path);
  }));
}

async function testPromises(target, path) {
  await fsPromises.symlink(target, path);
  fs.readdirSync(path);
}

for (const linkTarget of linkTargets) {
  fs.mkdirSync(tmpdir.resolve(linkTarget));
  for (const linkPath of linkPaths) {
    testSync(linkTarget, `${linkPath}-${path.basename(linkTarget)}-sync`);
    testAsync(linkTarget, `${linkPath}-${path.basename(linkTarget)}-async`);
    testPromises(linkTarget, `${linkPath}-${path.basename(linkTarget)}-promises`)
      .then(common.mustCall());
  }
}

// Test invalid symlink
{
  function testSync(target, path) {
    fs.symlinkSync(target, path);
    assert(!fs.existsSync(path));
  }

  function testAsync(target, path) {
    fs.symlink(target, path, common.mustSucceed(() => {
      assert(!fs.existsSync(path));
    }));
  }

  async function testPromises(target, path) {
    await fsPromises.symlink(target, path);
    assert(!fs.existsSync(path));
  }

  for (const linkTarget of linkTargets.map((p) => p + '-broken')) {
    for (const linkPath of linkPaths) {
      testSync(linkTarget, `${linkPath}-${path.basename(linkTarget)}-sync`);
      testAsync(linkTarget, `${linkPath}-${path.basename(linkTarget)}-async`);
      testPromises(linkTarget, `${linkPath}-${path.basename(linkTarget)}-promises`)
        .then(common.mustCall());
    }
  }
}
