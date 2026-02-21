'use strict';
const common = require('../common');
const assert = require('assert');
const child_process = require('child_process');

if (common.isWindows)
  common.skip('fs.closeSync(n) does not close stdio on Windows');

function runTest(fd, streamName, testOutputStream, expectedName) {
  const result = child_process.spawnSync(process.execPath, [
    '--expose-internals',
    '--no-warnings',
    '-e',
    `const { internalBinding } = require('internal/test/binding');
    internalBinding('process_methods').resetStdioForTesting();
    fs.closeSync(${fd});
    const ctorName = process.${streamName}.constructor.name;
    process.${testOutputStream}.write(ctorName);
    `]);
  assert.strictEqual(result[testOutputStream].toString(), expectedName,
                     `stdout:\n${result.stdout}\nstderr:\n${result.stderr}\n` +
                     `while running test with fd = ${fd}`);
  if (testOutputStream !== 'stderr')
    assert.strictEqual(result.stderr.toString(), '');
}

runTest(0, 'stdin', 'stdout', 'Readable');
runTest(1, 'stdout', 'stderr', 'Writable');
runTest(2, 'stderr', 'stdout', 'Writable');
