'use strict';

// The inspector and the node.async_hooks trace event category install async
// hooks internally. Neither of them should emit the DEP0212 deprecation
// warning meant for users of async_hooks.createHook().

const common = require('../common');
const assert = require('assert');
const { spawnSyncAndAssert } = require('../common/child_process');
const tmpdir = require('../common/tmpdir');

process.on('warning', common.mustNotCall());

let hasTraceEvents = true;
try {
  require('trace_events');
} catch {
  hasTraceEvents = false;
}

if (hasTraceEvents) {
  tmpdir.refresh();
  spawnSyncAndAssert(process.execPath, [
    '--trace-event-categories', 'node.async_hooks',
    '-e', 'setTimeout(() => {}, 1)',
  ], { cwd: tmpdir.path }, {
    stderr(output) {
      assert.doesNotMatch(output, /DEP0212/);
    },
  });
}

if (common.hasInspector) {
  const { Session } = require('inspector/promises');
  const session = new Session();
  session.connect();
  (async () => {
    await session.post('Debugger.enable');
    await session.post('Debugger.setAsyncCallStackDepth', { maxDepth: 10 });
    // Schedule some async work while the inspector async hook is enabled.
    await new Promise((resolve) => setTimeout(resolve, 1));
    session.disconnect();
  })().then(common.mustCall());
}
