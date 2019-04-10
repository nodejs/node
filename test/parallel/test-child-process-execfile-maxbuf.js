'use strict';
const common = require('../common');
const assert = require('assert');
const { execFile } = require('child_process');

function checkFactory(streamName) {
  return common.mustCall((err) => {
    assert(err instanceof RangeError);
    assert.strictEqual(err.message, `${streamName} maxBuffer length exceeded`);
    assert.strictEqual(err.code, 'ERR_CHILD_PROCESS_STDIO_MAXBUFFER');
  });
}

// default value
{
  execFile(
    process.execPath,
    ['-e', 'console.log("a".repeat(1024 * 1024))'],
    checkFactory('stdout')
  );
}

// default value
{
  execFile(
    process.execPath,
    ['-e', 'console.log("a".repeat(1024 * 1024 - 1))'],
    common.mustCall((err, stdout, stderr) => {
      assert.ifError(err);
      assert.strictEqual(stdout.trim(), 'a'.repeat(1024 * 1024 - 1));
      assert.strictEqual(stderr, '');
    })
  );
}

{
  const options = { maxBuffer: Infinity };

  execFile(
    process.execPath,
    ['-e', 'console.log("hello world");'],
    options,
    common.mustCall((err, stdout, stderr) => {
      assert.ifError(err);
      assert.strictEqual(stdout.trim(), 'hello world');
      assert.strictEqual(stderr, '');
    })
  );
}

{
  execFile('echo', ['hello world'], { maxBuffer: 5 }, checkFactory('stdout'));
}

const unicode = '中文测试'; // length = 4, byte length = 12

{
  execFile(
    process.execPath,
    ['-e', `console.log('${unicode}');`],
    { maxBuffer: 10 },
    checkFactory('stdout'));
}

{
  execFile(
    process.execPath,
    ['-e', `console.error('${unicode}');`],
    { maxBuffer: 10 },
    checkFactory('stderr')
  );
}

{
  const child = execFile(
    process.execPath,
    ['-e', `console.log('${unicode}');`],
    { encoding: null, maxBuffer: 10 },
    checkFactory('stdout')
  );

  child.stdout.setEncoding('utf-8');
}

{
  const child = execFile(
    process.execPath,
    ['-e', `console.error('${unicode}');`],
    { encoding: null, maxBuffer: 10 },
    checkFactory('stderr')
  );

  child.stderr.setEncoding('utf-8');
}
