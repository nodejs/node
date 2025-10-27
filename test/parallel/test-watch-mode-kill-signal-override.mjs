// Test that the kill signal sent by --watch can be overridden to SIGINT
// by using --watch-kill-signal.

import '../common/index.mjs';
import assert from 'node:assert';
import { writeFileSync } from 'node:fs';
import { spawn } from 'node:child_process';
import { once } from 'node:events';
import tmpdir from '../common/tmpdir.js';
import fixtures from '../common/fixtures.js';
import { skipIfNoWatchModeSignals } from '../common/watch.js';

skipIfNoWatchModeSignals();

tmpdir.refresh();
const indexPath = tmpdir.resolve('kill-signal-for-watch.js');
const indexContents = fixtures.readSync('kill-signal-for-watch.js', 'utf8');
writeFileSync(indexPath, indexContents);

const child = spawn(
  process.execPath,
  ['--watch', '--watch-kill-signal', 'SIGINT', indexPath],
  {
    cwd: tmpdir.path,
    stdio: ['inherit', 'pipe', 'inherit', 'ipc'],
  }
);

let stdout = '';
child.stdout.on('data', (data) => {
  const dataStr = data.toString();
  console.log(`[STDOUT] ${dataStr}`);
  stdout += `${dataStr}`;
  if (/__(SIGINT|SIGTERM) received__/.test(stdout)) {
    console.log(`[PARENT] Sending kill signal to child process: ${child.pid}`);
    child.kill();
  }
});

// After the write triggers a restart of the grandchild, the newly spawned second
// grandchild can post another 'script ready' message before the stdout from the first
// grandchild is relayed by the watcher and processed by this parent process to kill
// the watcher. If we write again and trigger another restart, we can
// end up in an infinite loop and never receive the stdout of the grandchildren in time.
// Only write once to verify the first grandchild process receives the expected signal.
// We don't care about the subsequent grandchild processes.
let written = false;
child.on('message', (msg) => {
  console.log(`[MESSAGE]`, msg);
  if (msg === 'script ready' && !written) {
    writeFileSync(indexPath, indexContents);
    written = true;
  }
});

await once(child, 'exit');

assert.match(stdout, /__SIGINT received__/);
assert.doesNotMatch(stdout, /__SIGTERM received__/);
