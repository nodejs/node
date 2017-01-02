'use strict';
const common = require('../../common');
const fs = require('fs');
const path = require('path');
const assert = require('assert');

if (common.isWOW64) {
  common.skip('doesn\'t work on WOW64');
  return;
}

common.refreshTmpDir();

// make a path that is more than 260 chars long.
// Any given folder cannot have a name longer than 260 characters,
// so create 10 nested folders each with 30 character long names.
var addonDestinationDir = path.resolve(common.tmpDir);

for (var i = 0; i < 10; i++) {
  addonDestinationDir = path.join(addonDestinationDir, 'x'.repeat(30));
  fs.mkdirSync(addonDestinationDir);
}

const addonPath = path.join(__dirname,
                            'build',
                            common.buildType,
                            'binding.node');
const addonDestinationPath = path.join(addonDestinationDir, 'binding.node');

// Copy binary to long path destination
var contents = fs.readFileSync(addonPath);
fs.writeFileSync(addonDestinationPath, contents);

// Attempt to load at long path destination
var addon = require(addonDestinationPath);
assert.notEqual(addon, null);
assert.strictEqual(addon.hello(), 'world');
