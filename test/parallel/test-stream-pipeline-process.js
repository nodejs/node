'use strict';

const common = require('../common');
const assert = require('assert');

if (process.argv[2] === 'child') {
  const { pipeline } = require('stream');
  pipeline(
    process.stdin,
    process.stdout,
    common.mustCall()
  );
} else {
  const cp = require('child_process');
  cp.exec([
    'echo',
    '"hello"',
    '|',
    `"${process.execPath}"`,
    `"${__filename}"`,
    'child'
  ].join(' '), common.mustCall((err) => {
    assert.ifError(err);
  }));
}
