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
let driveLetter;
for (var i = 0; i < driveLetters.length; ++i) {
  driveLetter = `${driveLetters[i]}:`;
  result = spawnSync('subst', [driveLetter, common.fixturesDir]);
  if (result.status === 0)
    break;
}
if (i === driveLetters.length) {
  common.skip('Cannot create subst drive');
  return;
}

var asyncCompleted = 0;
// schedule cleanup (and check if all callbacks where called)
process.on('exit', function() {
  spawnSync('subst', ['/d', driveLetter]);
  assert.equal(asyncCompleted, 2);
});


// test:
const filename = `${driveLetter}\\empty.js`;
const filenameBuffer = Buffer.from(filename);

result = fs.realpathSync(filename);
assert.equal(typeof result, 'string');
assert.equal(result, filename);

result = fs.realpathSync(filename, 'buffer');
assert(Buffer.isBuffer(result));
assert(result.equals(filenameBuffer));

fs.realpath(filename, function(err, result) {
  ++asyncCompleted;
  assert(!err);
  assert.equal(typeof result, 'string');
  assert.equal(result, filename);
});

fs.realpath(filename, 'buffer', function(err, result) {
  ++asyncCompleted;
  assert(!err);
  assert(Buffer.isBuffer(result));
  assert(result.equals(filenameBuffer));
});
