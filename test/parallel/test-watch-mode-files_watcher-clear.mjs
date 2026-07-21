// Flags: --expose-internals
import * as common from '../common/index.mjs';
import tmpdir from '../common/tmpdir.js';
import assert from 'node:assert';
import { writeFileSync } from 'node:fs';
import { createRequire } from 'node:module';

if (common.isIBMi)
  common.skip('IBMi does not support `fs.watch()`');

const require = createRequire(import.meta.url);
const timers = require('node:timers');
const originalSetTimeout = timers.setTimeout;
const originalClearTimeout = timers.clearTimeout;
const { promise, resolve } = Promise.withResolvers();
const debounce = 1000;
let debounceTimer;
let debounceTimerCallback;
let debounceTimerCleared = false;

timers.setTimeout = function(fn, delay, ...args) {
  // Only intercept the FilesWatcher debounce timer configured below.
  if (delay === debounce) {
    const timer = {
      __proto__: null,
      ref() { return this; },
      unref() { return this; },
    };
    debounceTimer = timer;
    debounceTimerCallback = () => {
      if (!debounceTimerCleared) {
        fn(...args);
      }
    };
    resolve();
    return timer;
  }
  return originalSetTimeout(fn, delay, ...args);
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

  const watcher = new FilesWatcher({ debounce, mode: 'all' });
  watcher.on('changed', common.mustNotCall());
  watcher.watchPath(file, false);

  const interval = setInterval(() => writeFileSync(file, `${Date.now()}`), 50);
  await promise;
  clearInterval(interval);

  watcher.clear();
  assert.strictEqual(debounceTimerCleared, true);
  debounceTimerCallback();
} finally {
  timers.setTimeout = originalSetTimeout;
  timers.clearTimeout = originalClearTimeout;
}
