'use strict';
const common = require('../../common');
if (common.isWindows && (process.env.PROCESSOR_ARCHITEW6432 !== undefined))
  common.skip('doesn\'t work on WOW64');

const fs = require('fs');
const path = require('path');
const assert = require('assert');
const { fork } = require('child_process');

const tmpdir = require('../../common/tmpdir');

// Make a path that is more than 260 chars long.
// Any given folder cannot have a name longer than 260 characters,
// so create 10 nested folders each with 30 character long names.
let addonDestinationDir = path.resolve(tmpdir.path);

for (let i = 0; i < 10; i++) {
  addonDestinationDir = path.join(addonDestinationDir, 'x'.repeat(30));
}

const addonPath = path.join(__dirname,
                            'build',
                            common.buildType,
                            'binding.node');
const addonDestinationPath = path.join(addonDestinationDir, 'binding.node');

// Loading an addon keeps the file open until the process terminates. Load
// the addon in a child process so that when the parent terminates the file
// is already closed and the tmpdir can be cleaned up.

// Child
if (process.argv[2] === 'child') {
  // Attempt to load at long path destination
  const addon = require(addonDestinationPath);
  assert.notStrictEqual(addon, null);
  assert.strictEqual(addon.hello(), 'world');
  return;
}

// Parent
tmpdir.refresh();

// Copy binary to long path destination
fs.mkdirSync(addonDestinationDir, { recursive: true });
const contents = fs.readFileSync(addonPath);
fs.writeFileSync(addonDestinationPath, contents);

// Run test
const child = fork(__filename, ['child'], { stdio: 'inherit' });
child.on('exit', common.mustCall((code) => {
  assert.strictEqual(code, 0);
}));
