// Flags: --expose-internals
import * as common from '../common/index.mjs';
import tmpdir from '../common/tmpdir.js';
import assert from 'node:assert';
import { writeFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import { setTimeout as sleep } from 'node:timers/promises';

if (common.isIBMi)
  common.skip('IBMi does not support `fs.watch()`');

const require = createRequire(import.meta.url);
const timers = require('node:timers');
const originalSetTimeout = timers.setTimeout;
const originalClearTimeout = timers.clearTimeout;
const { promise, resolve } = Promise.withResolvers();
let debounceTimer;
let debounceTimerCleared = false;

timers.setTimeout = function(fn, delay, ...args) {
  const timer = originalSetTimeout(fn, delay, ...args);
  if (delay === 1000) {
    debounceTimer = timer;
    debounceTimer.ref();
    resolve();
  }
  return timer;
};

timers.clearTimeout = function(timer) {
  if (timer === debounceTimer) {
    debounceTimerCleared = true;
  }
  return originalClearTimeout(timer);
};

try {
  const { FilesWatcher } = require('internal/watch_mode/files_watcher');

  tmpdir.refresh();
  const file = tmpdir.resolve('watcher-clear.js');
  writeFileSync(file, '0');

  const watcher = new FilesWatcher({ debounce: 1000, mode: 'all' });
  watcher.on('changed', common.mustNotCall());
  watcher.watchPath(file, false);

  const interval = setInterval(() => writeFileSync(file, `${Date.now()}`), 50);
  await promise;
  clearInterval(interval);

  watcher.clear();
  assert.strictEqual(debounceTimerCleared, true);
  await sleep(1100);
} finally {
  timers.setTimeout = originalSetTimeout;
  timers.clearTimeout = originalClearTimeout;
}
