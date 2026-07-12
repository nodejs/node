// Flags: --expose-internals
'use strict';

const common = require('../common');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');
const { isMainThread } = require('worker_threads');

common.skipIfPerfettoEnabled();
if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('assert');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');

try {
  require('trace_events');
} catch {
  common.skip('missing trace events');
}

if (!process.permission) {
  tmpdir.refresh();

  const allowed = tmpdir.resolve('allowed');
  const outside = tmpdir.resolve('outside');
  fs.mkdirSync(allowed);
  fs.mkdirSync(outside);

  spawnSyncAndExitWithoutError(process.execPath, [
    '--permission',
    '--allow-fs-read=*',
    `--allow-fs-write=${allowed}`,
    __filename,
    'child',
  ], { cwd: outside });
  return;
}

assert.strictEqual(process.argv[2], 'child');

assert.throws(() => {
  fs.writeFileSync('canary', 'x');
}, common.expectsError({
  code: 'ERR_ACCESS_DENIED',
  permission: 'FileSystemWrite',
}));

const tracing = require('trace_events').createTracing({
  categories: ['node', 'v8', 'node.perf'],
});

assert.throws(() => {
  tracing.enable();
}, common.expectsError({
  code: 'ERR_ACCESS_DENIED',
  permission: 'FileSystemWrite',
  resource: 'node_trace.1.log',
}));

assert.strictEqual(fs.existsSync('node_trace.1.log'), false);
