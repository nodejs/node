'use strict';
// Refs: https://github.com/nodejs/node/issues/2148

require('../common');
const assert = require('assert');
const execSync = require('child_process').execSync;

const lineSeed = 'foo bar baz quux quuz aaa bbb ccc';

if (process.argv[2] === 'child') {
  const longLine = lineSeed.repeat(parseInt(process.argv[4], 10));
  process.on('exit', () => {
    console.log(longLine);
  });
  process.exit();
}

[1, 2, 3, 4, 5, 6, 7, 7, 6, 5, 4, 3, 2, 1].forEach((exponent) => {
  const bigNum = Math.pow(2, exponent);
  const longLine = lineSeed.repeat(bigNum);
  const cmd =
    `"${process.execPath}" "${__filename}" child ${exponent} ${bigNum}`;
  const stdout = execSync(cmd).toString().trim();

  assert.strictEqual(stdout, longLine, `failed with exponent ${exponent}`);
});
