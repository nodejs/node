'use strict';
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

const unicode = '中文测试'; // length = 4, byte length = 12

{
  const cmd = `"${process.execPath}" -e "console.log('${unicode}');"`;

  cp.exec(cmd, {maxBuffer: 10}, checkFactory('stdout'));
}

{
  const cmd = `"${process.execPath}" -e "console.('${unicode}');"`;

  cp.exec(cmd, {maxBuffer: 10}, checkFactory('stderr'));
}
