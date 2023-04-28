'use strict';

const {
  Error,
  ObjectDefineProperty,
  ObjectGetOwnPropertyDescriptor,
  ObjectSetPrototypeOf,
  SafeArrayIterator,
  SafeSet,
  StringPrototypeStartsWith,
  StringPrototypeSlice,
} = primordials;

const binding = internalBinding('mksnapshot');
const { BuiltinModule } = require('internal/bootstrap/realm');
const {
  getEmbedderEntryFunction,
  compileSerializeMain,
} = binding;

const {
  getOptionValue,
} = require('internal/options');

const {
  readFileSync,
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
  'diagnostics_channel',
  'dns',
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
  'net',
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
  'zlib',
]));

const warnedModules = new SafeSet();
function supportedInUserSnapshot(id) {
  return supportedModules.has(id);
}

function requireForUserSnapshot(id) {
  let normalizedId = id;
  if (StringPrototypeStartsWith(id, 'node:')) {
    normalizedId = StringPrototypeSlice(id, 5);
  }
  if (!BuiltinModule.canBeRequiredByUsers(normalizedId) ||
      (id !== normalizedId &&
        !BuiltinModule.canBeRequiredWithoutScheme(normalizedId))) {
    // eslint-disable-next-line no-restricted-syntax
    const err = new Error(
      `Cannot find module '${id}'. `,
    );
    err.code = 'MODULE_NOT_FOUND';
    throw err;
  }
  if (!supportedInUserSnapshot(normalizedId)) {
    if (!warnedModules.has(normalizedId)) {
      process.emitWarning(
        `built-in module ${id} is not yet supported in user snapshots`);
      warnedModules.add(normalizedId);
    }
  }

  return require(normalizedId);
}

function main() {
  const {
    prepareMainThreadExecution,
  } = require('internal/process/pre_execution');
  const path = require('path');

  let serializeMainFunction = getEmbedderEntryFunction();
  const serializeMainArgs = [requireForUserSnapshot];

  if (serializeMainFunction) { // embedded case
    prepareMainThreadExecution(false, false);
    // TODO(addaleax): Make this `embedderRunCjs` once require('module')
    // is supported in snapshots.
    const filename = process.execPath;
    const dirname = path.dirname(filename);
    function minimalRunCjs(source) {
      const fn = compileSerializeMain(filename, source);
      return fn(requireForUserSnapshot, filename, dirname);
    }
    serializeMainArgs.push(minimalRunCjs);
  } else {
    prepareMainThreadExecution(true, false);
    const file = process.argv[1];
    const filename = path.resolve(file);
    const dirname = path.dirname(filename);
    const source = readFileSync(file, 'utf-8');
    serializeMainFunction = compileSerializeMain(filename, source);
    serializeMainArgs.push(filename, dirname);
  }

  const {
    initializeCallbacks,
    namespace: {
      addSerializeCallback,
      addDeserializeCallback,
    },
  } = require('internal/v8/startup_snapshot');
  initializeCallbacks();

  let stackTraceLimitDesc;
  addDeserializeCallback(() => {
    if (stackTraceLimitDesc !== undefined) {
      ObjectDefineProperty(Error, 'stackTraceLimit', stackTraceLimitDesc);
    }
  });

  if (getOptionValue('--inspect-brk')) {
    internalBinding('inspector').callAndPauseOnStart(
      serializeMainFunction, undefined, ...serializeMainArgs);
  } else {
    serializeMainFunction(...serializeMainArgs);
  }

  addSerializeCallback(() => {
    stackTraceLimitDesc = ObjectGetOwnPropertyDescriptor(Error, 'stackTraceLimit');

    if (stackTraceLimitDesc !== undefined) {
      // We want to use null-prototype objects to not rely on globally mutable
      // %Object.prototype%.
      ObjectSetPrototypeOf(stackTraceLimitDesc, null);
      process._rawDebug('Deleting Error.stackTraceLimit from the snapshot. ' +
                        'It will be re-installed after deserialization');
      delete Error.stackTraceLimit;
    }
  });
}

main();
