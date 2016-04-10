'use strict';
// Refs: https://github.com/nodejs/node/issues/2148

require('../common');
const assert = require('assert');
const execSync = require('child_process').execSync;

const longLine = 'foo bar baz quux quuz aaa bbb ccc'.repeat(65536);

if (process.argv[2] === 'child') {
  process.on('exit', () => {
    console.log(longLine);
  });
  process.exit();
}

const cmd = `${process.execPath} ${__filename} child`;
const stdout = execSync(cmd).toString().trim();

assert.strictEqual(stdout, longLine);
