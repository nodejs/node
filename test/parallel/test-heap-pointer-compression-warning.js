// Flags: --no-warnings --expose-internals --expose-gc
'use strict';

// Tests the HEAP_EXCEEDS_POINTER_COMPRESSION_LIMIT warning, which is emitted
// once per thread when the heap that survives a full GC would not fit in the
// 4 GiB pointer compression cage. It says the heap "is likely to exceed" the
// limit from 6.5 GiB and "will exceed" it from 8 GiB. The thresholds are
// lowered through an internal binding so the test does not need to allocate a
// large heap.

const common = require('../common');

common.skipIf32Bits();
if (process.config.variables.v8_enable_pointer_compression) {
  common.skip('the warning is only emitted without pointer compression');
}

const assert = require('assert');
const { internalBinding } = require('internal/test/binding');
const { spawnSyncAndAssert } = require('../common/child_process');
const { Worker, isMainThread, threadId } = require('worker_threads');

const kCode = 'HEAP_EXCEEDS_POINTER_COMPRESSION_LIMIT';
const kLikely = /is likely to exceed that limit, and the application may run out of memory/;
const kCertain = /will exceed that limit, and the application will run out of memory/;
const childFlags = ['--expose-internals', '--expose-gc'];

// Larger than any heap this test can have.
const kNever = Number.MAX_SAFE_INTEGER;

function triggerWarning(tier) {
  const { setPointerCompressionHeapWarningThresholds } = internalBinding('v8');
  if (tier === 'likely') {
    setPointerCompressionHeapWarningThresholds(1, kNever);
  } else {
    setPointerCompressionHeapWarningThresholds(1, 1);
  }
  globalThis.gc();
  // The warning is emitted from an unref'ed immediate, so keep the event loop
  // alive for one more iteration.
  setImmediate(() => {});
}

switch (process.argv[2]) {
  case 'child':
    triggerWarning(process.argv[3]);
    return;
  case 'worker': {
    const { parentPort } = require('worker_threads');
    process.on('warning', (warning) => {
      if (warning.code !== kCode) return;
      parentPort.postMessage({ name: warning.name, message: warning.message });
    });
    triggerWarning('certain');
    return;
  }
}

// Collect only this warning; requiring internal/test/binding emits its own.
const warnings = [];
process.on('warning', (warning) => {
  if (warning.code === kCode) warnings.push(warning);
});

// With the default thresholds, an ordinary heap does not trigger the warning.
globalThis.gc();
setImmediate(common.mustCall(() => {
  assert.strictEqual(warnings.length, 0);

  // A heap between the two thresholds gets the "likely" wording.
  triggerWarning('likely');

  setImmediate(common.mustCall(() => {
    // The warning is emitted only once per thread, and its wording is decided
    // when it is first triggered: a heap that later passes the "certain"
    // threshold does not emit a second warning.
    internalBinding('v8').setPointerCompressionHeapWarningThresholds(1, 1);
    globalThis.gc();
    setImmediate(() => {});
  }));
}));

process.on('exit', () => {
  assert.strictEqual(warnings.length, 1);
  const [warning] = warnings;
  assert.strictEqual(warning.name, 'Warning');
  // The test-worker suite runs this file inside a worker thread.
  const thread = isMainThread ? 'the main thread' : `worker thread ${threadId}`;
  assert.match(warning.message,
               new RegExp(`The V8 heap of ${thread} is using \\d+ MiB`));
  assert.match(warning.message, /limited to 4096 MiB/);
  assert.match(warning.message, kLikely);
  assert.doesNotMatch(warning.message, kCertain);
});

// A heap at or above the "certain" threshold gets the "will exceed" wording,
// and the default stderr output includes the code.
spawnSyncAndAssert(process.execPath, [
  ...childFlags,
  __filename,
  'child',
  'certain',
], {
  stderr(output) {
    assert.match(
      output,
      new RegExp(`\\[${kCode}\\] Warning: The V8 heap of the main thread`));
    assert.match(output, kCertain);
    assert.doesNotMatch(output, kLikely);
  },
});

// The code can be used to disable the warning.
spawnSyncAndAssert(process.execPath, [
  ...childFlags,
  `--disable-warning=${kCode}`,
  __filename,
  'child',
  'certain',
], {
  stderr(output) {
    assert.doesNotMatch(output, new RegExp(kCode));
  },
});

// Each worker thread has its own heap and its own warning.
const worker = new Worker(__filename, { argv: ['worker'] });
worker.on('message', common.mustCall((warning) => {
  assert.strictEqual(warning.name, 'Warning');
  assert.match(
    warning.message,
    new RegExp(`The V8 heap of worker thread ${worker.threadId} is using`));
  assert.match(warning.message, kCertain);
}));
