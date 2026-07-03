// Flags: --expose-internals
'use strict';

const common = require('../common');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');
const { isMainThread } = require('worker_threads');

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
  const traceFilePattern = tmpdir.resolve(
    'outside',
    // eslint-disable-next-line no-template-curly-in-string
    'denied-node-trace.${rotation}.log');
  const traceFile = tmpdir.resolve('outside', 'denied-node-trace.1.log');
  fs.mkdirSync(allowed);
  fs.mkdirSync(outside);

  spawnSyncAndExitWithoutError(process.execPath, [
    '--permission',
    '--allow-fs-read=*',
    `--allow-fs-write=${allowed}`,
    '--trace-event-file-pattern',
    traceFilePattern,
    __filename,
    'child',
    traceFile,
  ], { cwd: outside });
  return;
}

assert.strictEqual(process.argv[2], 'child');
const traceFile = process.argv[3];

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
  resource: traceFile,
}));

assert.strictEqual(fs.existsSync(traceFile), false);
