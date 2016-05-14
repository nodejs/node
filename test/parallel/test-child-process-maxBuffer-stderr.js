'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const unicode = '中文测试'; // Length = 4, Byte length = 13

if (process.argv[2] === 'child') {
  console.error(unicode);
} else {
  const cmd = `${process.execPath} ${__filename} child`;

  cp.exec(cmd, {maxBuffer: 10}, common.mustCall((err, stdout, stderr) => {
    assert.strictEqual(err.message, 'stderr maxBuffer exceeded');
  }));
}
