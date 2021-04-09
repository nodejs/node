'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

if (!common.isMainThread) {
  common.skip('process.chdir() is not available in workers');
}

const fixtures = require('../common/fixtures');
const startCLI = require('../common/inspector-cli');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();
process.chdir(tmpdir.path);

const { readFileSync } = require('fs');

const filename = 'node.heapsnapshot';

// Heap profiler take snapshot.
{
  const cli = startCLI([fixtures.path('inspector-cli/empty.js')]);

  function onFatal(error) {
    cli.quit();
    throw error;
  }

  // Check that the snapshot is valid JSON.
  return cli.waitForInitialBreak()
    .then(() => cli.waitForPrompt())
    .then(() => cli.command('takeHeapSnapshot()'))
    .then(() => JSON.parse(readFileSync(filename, 'utf8')))
    .then(() => cli.quit())
    .then(null, onFatal);
}
