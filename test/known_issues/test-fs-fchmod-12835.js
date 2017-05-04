'use strict';
const common = require('../common');

// Windows only: Test fchmod on files with `attrib -a -h -i -r -s`
// setting RO then can't set RW back

const {strictEqual, ok} = require('assert');
const {execSync} = require('child_process');
const fs = require('fs');

ok(common.isWindows, 'reset of test only for windows');

process.on('exit', function() {
  console.log('Trying to cleanup');
  try {
    execSync(`del /q /f ${tmpFile}`);
  } catch (e) {
    console.log(e);
  }
});

const RO = 0o100444; // 33060 0b1000000100100100
const RW = 0o100666; // 33206 0b1000000110110110

const tmpFile = `${common.tmpDir}\\${Date.now()}gigi.js`;

const fd = fs.openSync(tmpFile, 'a');
execSync(`attrib -a -h -i -r -s ${tmpFile}`);
fs.fchmodSync(fd, RO);
const actual2 = fs.fstatSync(fd).mode;
console.log(`${tmpFile} mode: ${actual2}`);
strictEqual(actual2, RO);

fs.fchmodSync(fd, RW); // This does not work.
const actual3 = fs.fstatSync(fd).mode;
console.log(`${tmpFile} mode: ${actual3}`);
strictEqual(actual3, RW); // BANG!
