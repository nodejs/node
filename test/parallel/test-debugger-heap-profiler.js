'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');
const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
const fs = require('node:fs');

tmpdir.refresh();


const filename = tmpdir.resolve('node.heapsnapshot');

// Heap profiler take snapshot.
{
  const opts = { cwd: tmpdir.path };
  const cli = startCLI(['--port=0', fixtures.path('debugger/empty.js')], [], opts);

  async function waitInitialBreak() {
    try {
      await cli.waitForInitialBreak();
      await cli.waitForPrompt();
      await cli.command('takeHeapSnapshot()');
      assert.ok(fs.existsSync(filename));
      // Check that two simultaneous snapshots don't step all over each other.
      // Refs: https://github.com/nodejs/node/issues/39555
      await cli.command('takeHeapSnapshot(); takeHeapSnapshot()');
      assert.ok(fs.existsSync(filename));
    } finally {
      await cli.quit();
    }
  }

  // Check that the snapshot is valid JSON.
  waitInitialBreak().then(common.mustCall());
}
