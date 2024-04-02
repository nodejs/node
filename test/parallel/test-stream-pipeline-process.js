'use strict';

const common = require('../common');
const assert = require('assert');
const os = require('os');

if (process.argv[2] === 'child') {
  const { pipeline } = require('stream');
  pipeline(
    process.stdin,
    process.stdout,
    common.mustSucceed()
  );
} else {
  const cp = require('child_process');
  cp.exec([
    'echo',
    'hello',
    '|',
    `"${process.execPath}"`,
    `"${__filename}"`,
    'child',
  ].join(' '), common.mustSucceed((stdout) => {
    assert.strictEqual(stdout.split(os.EOL).shift().trim(), 'hello');
  }));
}
