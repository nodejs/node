'use strict';
const common = require('../../common');
if (common.isWindows && (process.env.PROCESSOR_ARCHITEW6432 !== undefined))
  common.skip('doesn\'t work on WOW64');

const fs = require('fs');
const path = require('path');
const assert = require('assert');

const tmpdir = require('../../common/tmpdir');
tmpdir.refresh();

// make a path that is more than 260 chars long.
// Any given folder cannot have a name longer than 260 characters,
// so create 10 nested folders each with 30 character long names.
let addonDestinationDir = path.resolve(tmpdir.path);

for (let i = 0; i < 10; i++) {
  addonDestinationDir = path.join(addonDestinationDir, 'x'.repeat(30));
  fs.mkdirSync(addonDestinationDir);
}

const addonPath = path.join(__dirname,
                            'build',
                            common.buildType,
                            'binding.node');
const addonDestinationPath = path.join(addonDestinationDir, 'binding.node');

// Copy binary to long path destination
const contents = fs.readFileSync(addonPath);
fs.writeFileSync(addonDestinationPath, contents);

// Attempt to load at long path destination
const addon = require(addonDestinationPath);
assert.notStrictEqual(addon, null);
assert.strictEqual(addon.hello(), 'world');
