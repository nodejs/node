'use strict';
// Refs: https://github.com/nodejs/node/issues/2148
require('../common');
const assert = require('assert');
const execSync = require('child_process').execSync;

const longLine = 'foo bar baz quux quuz aaa bbb ccc'.repeat(80);
const expectedLength = (longLine.length * 999) + 1;

if (process.argv[2] === 'child') {
  process.on('exit', () => {
    console.log(longLine.repeat(999));
  });
  process.exit();
}

const stdout = execSync(`${process.execPath} ${__filename} child`);

assert.strictEqual(stdout.length, expectedLength);
