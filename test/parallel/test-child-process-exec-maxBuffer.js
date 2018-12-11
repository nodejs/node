'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');

function runChecks(err, stdio, streamName, expected) {
  assert.strictEqual(err.message, `${streamName} maxBuffer length exceeded`);
  assert(err instanceof RangeError);
  assert.strictEqual(err.code, 'ERR_CHILD_PROCESS_STDIO_MAXBUFFER');
  assert.deepStrictEqual(stdio[streamName], expected);
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

  cp.exec(
    cmd,
    { maxBuffer: 5 },
    common.mustCall((err, stdout, stderr) => {
      runChecks(err, { stdout, stderr }, 'stdout', '');
    })
  );
}

const unicode = '中文测试'; // length = 4, byte length = 12

{
  const cmd = `"${process.execPath}" -e "console.log('${unicode}');"`;

  cp.exec(
    cmd,
    { maxBuffer: 10 },
    common.mustCall((err, stdout, stderr) => {
      runChecks(err, { stdout, stderr }, 'stdout', '');
    })
  );
}

{
  const cmd = `"${process.execPath}" -e "console.error('${unicode}');"`;

  cp.exec(
    cmd,
    { maxBuffer: 3 },
    common.mustCall((err, stdout, stderr) => {
      runChecks(err, { stdout, stderr }, 'stderr', '');
    })
  );
}

{
  const cmd = `"${process.execPath}" -e "console.log('${unicode}');"`;

  const child = cp.exec(
    cmd,
    { encoding: null, maxBuffer: 10 },
    common.mustCall((err, stdout, stderr) => {
      runChecks(err, { stdout, stderr }, 'stdout', '');
    })
  );

  child.stdout.setEncoding('utf-8');
}

{
  const cmd = `"${process.execPath}" -e "console.error('${unicode}');"`;

  const child = cp.exec(
    cmd,
    { encoding: null, maxBuffer: 3 },
    common.mustCall((err, stdout, stderr) => {
      runChecks(err, { stdout, stderr }, 'stderr', '');
    })
  );

  child.stderr.setEncoding('utf-8');
}
