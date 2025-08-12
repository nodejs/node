'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const { readFileSync } = require('fs');

const filename = tmpdir.resolve('node.heapsnapshot');

// Heap profiler take snapshot.
{
  const opts = { cwd: tmpdir.path };
  const cli = startCLI([fixtures.path('debugger/empty.js')], [], opts);

  async function waitInitialBreak() {
    try {
      await cli.waitForInitialBreak();
      await cli.waitForPrompt();
      await cli.command('takeHeapSnapshot()');
      JSON.parse(readFileSync(filename, 'utf8'));
      // Check that two simultaneous snapshots don't step all over each other.
      // Refs: https://github.com/nodejs/node/issues/39555
      await cli.command('takeHeapSnapshot(); takeHeapSnapshot()');
      JSON.parse(readFileSync(filename, 'utf8'));
    } finally {
      await cli.quit();
    }
  }

  // Check that the snapshot is valid JSON.
  waitInitialBreak().then(common.mustCall());
}
