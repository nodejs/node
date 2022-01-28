import { skipIfInspectorDisabled } from '../common/index.mjs';
import { createRequire } from 'module';
import path from 'path';

const require = createRequire(import.meta.url);
const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');
const tmpdir = require('../common/tmpdir');

skipIfInspectorDisabled();

tmpdir.refresh();

const { readFileSync } = require('fs');

const filename = path.join(tmpdir.path, 'node.heapsnapshot');

// Heap profiler take snapshot.
const opts = { cwd: tmpdir.path };
const cli = startCLI([fixtures.path('debugger/empty.js')], [], opts);

try {
  // Check that the snapshot is valid JSON.
  await cli.waitForInitialBreak();
  await cli.waitForPrompt();
  await cli.command('takeHeapSnapshot()');
  await JSON.parse(readFileSync(filename, 'utf8'));
  // Check that two simultaneous snapshots don't step all over each other.
  // Refs: https://github.com/nodejs/node/issues/39555
  await cli.command('takeHeapSnapshot(); takeHeapSnapshot()');
  await JSON.parse(readFileSync(filename, 'utf8'));
  await cli.quit();
} catch (e) {
  cli.quit();
  throw e;
}
