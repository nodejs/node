// Flags: --no-async-context-frame --expose-internals
// Verify that the runtime warning fired when async-context-frame is disabled
// names the correct flag (--no-async-context-frame) rather than the obsolete
// --experimental-async-context-frame, which does not exist in Node.js 27.
'use strict';

const common = require('../common');
const assert = require('assert');
const v8 = require('v8');
const { internalBinding } = require('internal/test/binding');

const { getProfilingAllocatorActive } = internalBinding('v8');
if (typeof getProfilingAllocatorActive !== 'function') {
  // Build does not have V8_HEAP_PROFILER_SAMPLE_LABELS; warning is a no-op.
  process.exit(0);
}

// Use process.on (not process.once): internal/test/binding emits its own
// warning on load which fires before ours and would capture a process.once
// listener first.
const captured = [];
process.on('warning', (w) => {
  if (w.code === 'NODE_HEAP_PROFILE_LABELS_NO_ASYNC_CONTEXT') captured.push(w);
});

// Trigger ensureHeapProfileLabelsALS, which emits the warning.
v8.setHeapProfileLabels({ route: '/test' });

// Warnings are emitted asynchronously; setImmediate runs after microtasks.
setImmediate(common.mustCall(() => {
  assert.strictEqual(captured.length, 1, 'expected exactly one label warning');
  assert.match(captured[0].message, /async-context-frame/);
  assert.match(captured[0].message, /--no-async-context-frame/);
}));
