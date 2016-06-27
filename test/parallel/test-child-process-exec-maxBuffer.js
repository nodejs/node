'use strict';
// Refs: https://github.com/nodejs/node/issues/1901
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');

function checkFactory(streamName) {
  return common.mustCall((err) => {
    const message = `${streamName} maxBuffer exceeded`;
    assert.strictEqual(err.message, message);
  });
}

{
  const cmd = 'echo "hello world"';

  cp.exec(cmd, { maxBuffer: 5 }, checkFactory('stdout'));
}

{
  const unicode = '中文测试'; // Length = 4, Byte length = 12
  const cmd = `echo ${unicode}`;

  cp.exec(cmd, {maxBuffer: 10}, checkFactory('stdout'));
}

{
  const unicode = '中文测试'; // Length = 4, Byte length = 12
  const cmd = `echo ${unicode} 1>&2`;

  cp.exec(cmd, {maxBuffer: 10}, checkFactory('stderr'));
}
