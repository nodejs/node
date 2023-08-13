'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const stdoutData = 'foo';
const stderrData = 'bar';
const expectedStdout = `${stdoutData}\n`;
const expectedStderr = `${stderrData}\n`;

if (process.argv[2] === 'child') {
  // The following console calls are part of the test.
  console.log(stdoutData);
  console.error(stderrData);
} else {
  const cmd = '"$NODE" "$FILE" child';
  const env = { NODE: process.execPath, FILE: __filename };
  const child = cp.exec(cmd, { env }, common.mustSucceed((stdout, stderr) => {
    assert.strictEqual(stdout, expectedStdout);
    assert.strictEqual(stderr, expectedStderr);
  }));
  child.stdout.setEncoding('utf-8');
  child.stderr.setEncoding('utf-8');
}
