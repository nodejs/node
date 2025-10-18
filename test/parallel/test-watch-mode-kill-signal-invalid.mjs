// Test that --watch-kill-signal errors when an invalid kill signal is provided.

import '../common/index.mjs';
import assert from 'node:assert';
import { writeFileSync } from 'node:fs';
import { spawn } from 'node:child_process';
import tmpdir from '../common/tmpdir.js';
import fixtures from '../common/fixtures.js';
import { skipIfNoWatchModeSignals } from '../common/watch.js';

skipIfNoWatchModeSignals();

tmpdir.refresh();
const indexPath = tmpdir.resolve('kill-signal-for-watch.js');
const indexContents = fixtures.readSync('kill-signal-for-watch.js', 'utf8');
writeFileSync(indexPath, indexContents);

const currentRun = Promise.withResolvers();
const child = spawn(
  process.execPath,
  ['--watch', '--watch-kill-signal', 'invalid_signal', indexPath],
  {
    cwd: tmpdir.path,
    stdio: ['inherit', 'inherit', 'pipe'],
  }
);
let stderr = '';

child.stderr.on('data', (data) => {
  stderr += data.toString();
  currentRun.resolve();
});

await currentRun.promise;

assert.match(
  stderr,
  /TypeError \[ERR_UNKNOWN_SIGNAL\]: Unknown signal: invalid_signal/
);
