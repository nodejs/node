'use strict';
// Refs: https://github.com/nodejs/node/issues/1901
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const unicode = '中文测试'; // Length = 4, Byte length = 13

if (process.argv[2] === 'stdout') {
  console.log(unicode);
  return;
}

if (process.argv[2] === 'stderr') {
  console.error(unicode);
  return;
}

const cmd = `${process.execPath} ${__filename}`;
const outputs = ['stdout', 'stderr'];

outputs.forEach((stream) => {
  const cb = common.mustCall((err, stdout, stderr) => {
    assert.strictEqual(err.message, `${stream} maxBuffer exceeded`);
  });

  cp.exec(`${cmd} ${stream}`, {maxBuffer: 10}, cb);
});
