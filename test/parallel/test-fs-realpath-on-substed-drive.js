'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const spawnSync = require('child_process').spawnSync;

if (!common.isWindows) {
  common.skip('Test for Windows only');
  return;
}
let result;

// create a subst drive
const driveLetters = 'ABCDEFGHIJKLMNOPQRSTUWXYZ';
let drive;
for (var i = 0; i < driveLetters.length; ++i) {
  drive = `${driveLetters[i]}:`;
  result = spawnSync('subst', [drive, common.fixturesDir]);
  if (result.status === 0)
    break;
}
if (i === driveLetters.length) {
  common.skip('Cannot create subst drive');
  return;
}

// schedule cleanup (and check if all callbacks where called)
process.on('exit', function() {
  spawnSync('subst', ['/d', drive]);
});

// test:
const filename = `${drive}\\empty.js`;
const filenameBuffer = Buffer.from(filename);

result = fs.realpathSync(filename);
assert.strictEqual(result, filename);

result = fs.realpathSync(filename, 'buffer');
assert(Buffer.isBuffer(result));
assert(result.equals(filenameBuffer));

fs.realpath(filename, common.mustCall(function(err, result) {
  assert(!err);
  assert.strictEqual(result, filename);
}));

fs.realpath(filename, 'buffer', common.mustCall(function(err, result) {
  assert(!err);
  assert(Buffer.isBuffer(result));
  assert(result.equals(filenameBuffer));
}));
