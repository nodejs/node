'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');
const tmpdir = require('../common/tmpdir');
const path = require('path');

tmpdir.refresh();

const { readFileSync } = require('fs');

const filename = path.join(tmpdir.path, 'node.heapsnapshot');

// Heap profiler take snapshot.
{
  const opts = { cwd: tmpdir.path };
  const cli = startCLI([fixtures.path('debugger/empty.js')], [], opts);

  function onFatal(error) {
    cli.quit();
    throw error;
  }

  // Check that the snapshot is valid JSON.
  return cli.waitForInitialBreak()
    .then(() => cli.waitForPrompt())
    .then(() => cli.command('takeHeapSnapshot()'))
    .then(() => JSON.parse(readFileSync(filename, 'utf8')))
    // Check that two simultaneous snapshots don't step all over each other.
    // Refs: https://github.com/nodejs/node/issues/39555
    .then(() => cli.command('takeHeapSnapshot(); takeHeapSnapshot()'))
    .then(() => JSON.parse(readFileSync(filename, 'utf8')))
    .then(() => cli.quit())
    .then(null, onFatal);
}
