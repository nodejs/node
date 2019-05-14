'use strict';
const { test } = require('tap');
const { readFileSync, unlinkSync } = require('fs');

const startCLI = require('./start-cli');
const filename = 'node.heapsnapshot';

function cleanup() {
  try {
    unlinkSync(filename);
  } catch (_) {
    // Ignore.
  }
}

cleanup();

test('Heap profiler take snapshot', (t) => {
  const cli = startCLI(['examples/empty.js']);

  function onFatal(error) {
    cli.quit();
    throw error;
  }

  // Check that the snapshot is valid JSON.
  return cli.waitForInitialBreak()
    .then(() => cli.waitForPrompt())
    .then(() => cli.command('takeHeapSnapshot()'))
    .then(() => JSON.parse(readFileSync(filename, 'utf8')))
    .then(() => cleanup())
    .then(() => cli.quit())
    .then(null, onFatal);
});
