'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');

function checkFactory(streamName) {
  return common.mustCall((err) => {
    assert.strictEqual(err.message, `${streamName} maxBuffer length exceeded`);
    assert(err instanceof RangeError);
    assert.strictEqual(err.code, 'ERR_CHILD_PROCESS_STDIO_MAXBUFFER');
  });
}

{
  const cmd = `"${process.execPath}" -e "console.log('hello world');"`;
  const options = { maxBuffer: Infinity };

  cp.exec(cmd, options, common.mustCall((err, stdout, stderr) => {
    assert.ifError(err);
    assert.strictEqual(stdout.trim(), 'hello world');
    assert.strictEqual(stderr, '');
  }));
}

{
  const cmd = 'echo "hello world"';

  cp.exec(cmd, { maxBuffer: 5 }, checkFactory('stdout'));
}

const unicode = '中文测试'; // length = 4, byte length = 12

{
  const cmd = `"${process.execPath}" -e "console.log('${unicode}');"`;

  cp.exec(cmd, { maxBuffer: 10 }, checkFactory('stdout'));
}

{
  const cmd = `"${process.execPath}" -e "console.error('${unicode}');"`;

  cp.exec(cmd, { maxBuffer: 10 }, checkFactory('stderr'));
}
