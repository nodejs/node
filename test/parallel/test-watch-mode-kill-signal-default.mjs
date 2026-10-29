// Test that the kill signal sent by --watch defaults to SIGTERM.

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
  ['--watch', indexPath],
  {
    cwd: tmpdir.path,
    stdio: ['inherit', 'pipe', 'inherit', 'ipc'],
  }
);

let stdout = '';
let firstGrandchildPid;
child.stdout.on('data', (data) => {
  const dataStr = data.toString();
  console.log(`[STDOUT] ${dataStr}`);
  stdout += `${dataStr}`;
  const match = dataStr.match(/__(SIGINT|SIGTERM) received__ (\d+)/);
  if (match && match[2] === firstGrandchildPid) {
    console.log(`[PARENT] Sending kill signal to watcher process: ${child.pid}`);
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
child.on('message', (msg) => {
  console.log(`[MESSAGE]`, msg);
  if (!firstGrandchildPid && typeof msg === 'string') {
    const match = msg.match(/script ready (\d+)/);
    if (match) {
      firstGrandchildPid = match[1];  // This is the first grandchild
      const writeDelay = 1000;  // Delay to reduce the chance of fs events coalescing
      console.log(`[PARENT] writing to restart ${firstGrandchildPid} after ${writeDelay}ms`);
      setTimeout(() => {
        console.log(`[PARENT] writing to ${indexPath} to restart ${firstGrandchildPid}`);
        writeFileSync(indexPath, indexContents);
      }, writeDelay);
    }
  }
});

await once(child, 'exit');

assert.match(stdout, new RegExp(`__SIGTERM received__ ${RegExp.escape(firstGrandchildPid)}`));
assert.doesNotMatch(stdout, new RegExp(`__SIGINT received__ ${RegExp.escape(firstGrandchildPid)}`));
