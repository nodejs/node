'use strict';

const {
  SafeSet,
  SafeArrayIterator
} = primordials;

const {
  compileSnapshotMain
} = internalBinding('mksnapshot');

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
  // 'crypto',
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

function supportedInUserSnapshot(id) {
  return supportedModules.has(id);
}

const warnedModules = new SafeSet();

function requireForUserSnapshot(id) {
  if (supportedInUserSnapshot(id)) {
    if (!warnedModules.has(id)) {
      process.emitWarning(
        `built-in module ${id} is not yet supported in user snapshots`);
      warnedModules.add(id);
    }
  }

  return require(id);
}

// prepareMainThreadExecution(true);

const mainFile = readFileSync(getOptionValue('--snapshot-main'), 'utf-8');

const snapshotMainFunction = compileSnapshotMain(mainFile);

snapshotMainFunction(requireForUserSnapshot);
