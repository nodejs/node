'use strict';
const common = require('../../common');
const fs = require('fs');
const path = require('path');
const assert = require('assert');

common.refreshTmpDir();

var destDir = path.resolve(common.tmpDir);

var location = [path.join(destDir, 'first'), path.join(destDir, 'second')];
fs.mkdirSync(location[0]);
try {
  fs.symlinkSync(location[0], location[1]);
} catch (EPERM) {
  console.log('1..0 # Skipped: module identity test (no privs for symlinks)');
  return;
}

const addonPath = path.join(__dirname, 'build', 'Release', 'binding.node');

var contents = fs.readFileSync(addonPath);
fs.writeFileSync(path.join(location[0], 'binding.node'), contents);
fs.writeFileSync(path.join(location[1], 'binding.node'), contents);

var primary = require(path.join(location[0], 'binding.node'));
assert(primary != null);
assert(primary.hello() == 'world');
var secondary = require(path.join(location[1], 'binding.node'));
assert(secondary != null);
assert(secondary.hello() == 'world');
require('./submodule').test(location[1]);
