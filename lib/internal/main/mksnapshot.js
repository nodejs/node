'use strict';

const {
  Error,
  SafeSet,
  SafeArrayIterator
} = primordials;

const binding = internalBinding('mksnapshot');
const { NativeModule } = require('internal/bootstrap/loaders');
const {
  compileSnapshotMain,
} = binding;

const {
  getOptionValue
} = require('internal/options');

const {
  readFileSync
} = require('fs');

const supportedModules = new SafeSet(new SafeArrayIterator([
  // '_http_agent',
  // '_http_client',
  // '_http_common',
  // '_http_incoming',
  // '_http_outgoing',
  // '_http_server',
  '_stream_duplex',
  '_stream_passthrough',
  '_stream_readable',
  '_stream_transform',
  '_stream_wrap',
  '_stream_writable',
  // '_tls_common',
  // '_tls_wrap',
  'assert',
  'assert/strict',
  // 'async_hooks',
  'buffer',
  // 'child_process',
  // 'cluster',
  'console',
  'constants',
  'crypto',
  // 'dgram',
  // 'diagnostics_channel',
  // 'dns',
  // 'dns/promises',
  // 'domain',
  'events',
  'fs',
  'fs/promises',
  // 'http',
  // 'http2',
  // 'https',
  // 'inspector',
  // 'module',
  // 'net',
  'os',
  'path',
  'path/posix',
  'path/win32',
  // 'perf_hooks',
  'process',
  'punycode',
  'querystring',
  // 'readline',
  // 'repl',
  'stream',
  'stream/promises',
  'string_decoder',
  'sys',
  'timers',
  'timers/promises',
  // 'tls',
  // 'trace_events',
  // 'tty',
  'url',
  'util',
  'util/types',
  'v8',
  // 'vm',
  // 'worker_threads',
  // 'zlib',
]));

const warnedModules = new SafeSet();
function supportedInUserSnapshot(id) {
  return supportedModules.has(id);
}

function requireForUserSnapshot(id) {
  if (!NativeModule.canBeRequiredByUsers(id)) {
    // eslint-disable-next-line no-restricted-syntax
    const err = new Error(
      `Cannot find module '${id}'. `
    );
    err.code = 'MODULE_NOT_FOUND';
    throw err;
  }
  if (!supportedInUserSnapshot(id)) {
    if (!warnedModules.has(id)) {
      process.emitWarning(
        `built-in module ${id} is not yet supported in user snapshots`);
      warnedModules.add(id);
    }
  }

  return require(id);
}

function main() {
  const {
    prepareMainThreadExecution
  } = require('internal/bootstrap/pre_execution');

  prepareMainThreadExecution(true, false);
  process.once('beforeExit', function runCleanups() {
    for (const cleanup of binding.cleanups) {
      cleanup();
    }
  });

  const file = process.argv[1];
  const path = require('path');
  const filename = path.resolve(file);
  const dirname = path.dirname(filename);
  const source = readFileSync(file, 'utf-8');
  const snapshotMainFunction = compileSnapshotMain(filename, source);

  if (getOptionValue('--inspect-brk')) {
    internalBinding('inspector').callAndPauseOnStart(
      snapshotMainFunction, undefined,
      requireForUserSnapshot, filename, dirname);
  } else {
    snapshotMainFunction(requireForUserSnapshot, filename, dirname);
  }
}

main();
