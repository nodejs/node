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
child.stdout.on('data', (data) => {
  stdout += `${data}`;
  if (/__(SIGINT|SIGTERM) received__/.test(stdout)) {
    child.kill();
  }
});

child.on('message', (msg) => {
  if (msg === 'script ready') {
    writeFileSync(indexPath, indexContents);
  }
});

await once(child, 'exit');

assert.match(stdout, /__SIGTERM received__/);
assert.doesNotMatch(stdout, /__SIGINT received__/);
