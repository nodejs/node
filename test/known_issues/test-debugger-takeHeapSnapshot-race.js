'use strict';
const common = require('../common');

// Refs: https://github.com/nodejs/node/issues/39555

// After this issue is fixed, this can perhaps be integrated into
// test/sequential/test-debugger-heap-profiler.js as it shares almost all
// the same code.

// These skips should be uncommented once the issue is fixed.
// common.skipIfInspectorDisabled();

// if (!common.isMainThread) {
//   common.skip('process.chdir() is not available in workers');
// }

// This assert.fail() can be removed once the issue is fixed.
if (!common.hasCrypto || !process.features.inspector) {
  require('assert').fail('crypto is not available');
}

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();
process.chdir(tmpdir.path);

const { readFileSync } = require('fs');

const filename = 'node.heapsnapshot';

// Check that two simultaneous snapshots don't step all over each other.
{
  const cli = startCLI([fixtures.path('debugger/empty.js')]);

  function onFatal(error) {
    cli.quit();
    throw error;
  }

  return cli.waitForInitialBreak()
    .then(() => cli.waitForPrompt())
    .then(() => cli.command('takeHeapSnapshot(); takeHeapSnapshot()'))
    .then(() => JSON.parse(readFileSync(filename, 'utf8')))
    .then(() => cli.quit())
    .then(null, onFatal);
}
