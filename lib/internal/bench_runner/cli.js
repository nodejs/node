'use strict';

const {
  ArrayFrom,
  ArrayIsArray,
  ArrayPrototypeFilter,
  ArrayPrototypeIncludes,
  ArrayPrototypeJoin,
  ArrayPrototypePush,
  ArrayPrototypePushApply,
  ArrayPrototypeSlice,
  ArrayPrototypeSome,
  ArrayPrototypeSort,
  MathMax,
  NumberIsFinite,
  NumberIsSafeInteger,
  ObjectGetOwnPropertyDescriptor,
  ObjectGetPrototypeOf,
  ObjectKeys,
  ObjectPrototype,
  ObjectPrototypeHasOwnProperty,
  ObjectValues,
  Promise,
  PromisePrototypeThen,
  PromiseReject,
  PromiseResolve,
  PromiseWithResolvers,
  RegExp,
  SafeMap,
  SafePromiseAllReturnVoid,
  SafeSet,
  String,
  StringPrototypeIndexOf,
  StringPrototypeReplaceAll,
  StringPrototypeSlice,
  StringPrototypeStartsWith,
  StringPrototypeToUpperCase,
  SymbolDispose,
} = primordials;
const { spawn } = require('child_process');
const { createWriteStream, statSync } = require('fs');
const { Glob } = require('internal/fs/glob');
const {
  BenchmarksStream,
} = require('internal/bench_runner/benchmarks_stream');
const {
  configureRunScope,
  createRunId,
  runInFileScope,
  runBenchmarks,
} = require('internal/bench_runner/harness');
const { deserializeError, serializeError } = require('internal/error_serdes');
const {
  AbortError,
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_STATE,
  },
} = require('internal/errors');
const { addAbortListener } = require('internal/events/abort_listener');
const {
  getCLIOptionsInfo,
  getOptionValue,
  getOptionsAsFlagsFromBinding,
} = require('internal/options');
const { TIMEOUT_MAX } = require('internal/timers');
const { kEmptyObject } = require('internal/util');
const {
  validateAbortSignal,
  validateArray,
  validateObject,
  validateStringWithoutNullBytes,
  validateUint32,
} = require('internal/validators');
const { pathToFileURL } = require('internal/url');
const { pipeline } = require('stream/promises');
const { isAbsolute, resolve, sep } = require('path');
const { clearTimeout, setTimeout } = require('timers');

const console = require('internal/console/global');
const esmLoader = require('internal/modules/esm/loader');
const { bigint: hrtime } = process.hrtime;

const kBuiltinDestinations = new SafeMap([
  ['stdout', process.stdout],
  ['stderr', process.stderr],
]);
const kBuiltinReporters = new SafeMap([
  ['json', 'internal/bench_runner/reporter/json'],
  ['spec', 'internal/bench_runner/reporter/spec'],
]);
const kChildAckMessageType = 'node:bench:ack';
const kChildMessageType = 'node:bench:record';
const kEventTypes = new SafeSet([
  'bench:plan',
  'bench:start',
  'bench:sample',
  'bench:complete',
  'bench:diagnostic',
  'bench:summary',
]);
const kFilterArgs = ['--bench', '--experimental-default-config-file'];
const kFilterArgValues = [
  '--bench-isolation',
  '--bench-name-pattern',
  '--bench-reporter',
  '--bench-reporter-destination',
  '--bench-samples',
  '--bench-warmup',
  '--experimental-config-file',
];
const kIncompatibleExecArgv = new SafeSet([
  '--build-sea',
  '--build-snapshot',
  '--build-snapshot-config',
  '--check',
  '--completion-bash',
  '--eval',
  '--experimental-sea-config',
  '--help',
  '--help-all',
  '--input-type',
  '--interactive',
  '--print',
  '--prof-process',
  '--run',
  '--test',
  '--version',
  '--v8-options',
  '--watch',
  '--watch-path',
  '-c',
  '-e',
  '-h',
  '-i',
  '-p',
  '-v',
]);
const kExecArgvWithValue = new SafeSet([
  '--eval',
  '--input-type',
  '--print',
  '--run',
  '--watch-path',
  '-e',
  '-p',
]);
const kIPCEnvironmentVariables = new SafeSet([
  'NODE_CHANNEL_FD',
  'NODE_CHANNEL_SERIALIZATION_MODE',
  'NODE_BENCH_CONTEXT',
  'NODE_BENCH_FILE_RUN_ID',
  'NODE_BENCH_RUN_ID',
  'NODE_OPTIONS',
]);
const kForceKillDelay = 1_000;

function createBenchmarkFileList(patterns, cwd) {
  if (patterns.length === 0) {
    console.error('--bench requires at least one file or glob pattern');
    return null;
  }

  const glob = new Glob(patterns, { __proto__: null, cwd });
  const files = ArrayPrototypeFilter(glob.globSync(), (path) => {
    try {
      return statSync(resolve(cwd, path)).isFile();
    } catch {
      return false;
    }
  });
  if (files.length === 0) {
    console.error(`Could not find '${ArrayPrototypeJoin(patterns, ', ')}'`);
    return null;
  }
  return ArrayPrototypeSort(files);
}

function createChildFileList(patterns, cwd) {
  if (patterns.length !== 1) {
    console.error('benchmark child process requires exactly one file');
    return null;
  }
  const path = resolve(cwd, patterns[0]);
  try {
    if (statSync(path).isFile()) return [path];
  } catch {
    // Fall through to the standard not-found diagnostic.
  }
  console.error(`Could not find '${patterns[0]}'`);
  return null;
}

function createFileScopes(files, options) {
  const scopes = [];
  for (let i = 0; i < files.length; i++) {
    ArrayPrototypePush(scopes, {
      __proto__: null,
      entryFile: resolve(options.cwd, files[i]),
      fileRunId: options.isChild && i === 0 &&
        options.fileRunId !== undefined ? options.fileRunId : createRunId(),
    });
  }
  return scopes;
}

function parseNamePattern(value) {
  if (value.length === 0) return undefined;
  try {
    return new RegExp(value);
  } catch (error) {
    throw new ERR_INVALID_ARG_VALUE(
      '--bench-name-pattern', value,
      `is an invalid regular expression. ${error.message}`);
  }
}

function parseCommandLine() {
  const reporters = getOptionValue('--bench-reporter');
  const destinations = getOptionValue('--bench-reporter-destination');
  const isChild = process.env.NODE_BENCH_CONTEXT === 'child' &&
    typeof process.send === 'function';

  if (!isChild) {
    if (reporters.length === 0 && destinations.length === 0) {
      ArrayPrototypePush(reporters, 'spec');
    }
    if (reporters.length === 1 && destinations.length === 0) {
      ArrayPrototypePush(destinations, 'stdout');
    }
    if (reporters.length !== destinations.length) {
      throw new ERR_INVALID_ARG_VALUE(
        '--bench-reporter', reporters,
        'must match the number of specified ' +
        "'--bench-reporter-destination'");
    }
  }

  const samples = getOptionValue('[has_bench_samples]') ?
    getOptionValue('--bench-samples') : undefined;
  const warmup = getOptionValue('[has_bench_warmup]') ?
    getOptionValue('--bench-warmup') : undefined;
  if (samples !== undefined) {
    validateUint32(samples, '--bench-samples', true);
  }
  if (warmup !== undefined) {
    validateUint32(warmup, '--bench-warmup');
  }

  return {
    __proto__: null,
    cwd: process.cwd(),
    destinations,
    fileRunId: isChild && process.env.NODE_BENCH_FILE_RUN_ID ?
      process.env.NODE_BENCH_FILE_RUN_ID : undefined,
    isChild,
    isolation: getOptionValue('--bench-isolation'),
    namePattern: parseNamePattern(getOptionValue('--bench-name-pattern')),
    namePatternSource: getOptionValue('--bench-name-pattern'),
    reporters,
    runId: isChild && process.env.NODE_BENCH_RUN_ID ?
      process.env.NODE_BENCH_RUN_ID : undefined,
    samples,
    warmup,
  };
}

async function loadReporter(name) {
  const builtin = kBuiltinReporters.get(name);
  let reporter;
  if (builtin !== undefined) {
    reporter = require(builtin);
  } else {
    let parentURL;
    try {
      parentURL = pathToFileURL(`${process.cwd()}${sep}`).href;
    } catch {
      parentURL = 'file:///';
    }
    const loader = esmLoader.getOrInitializeCascadedLoader();
    reporter = await loader.import(name, parentURL, kEmptyObject);
  }

  reporter = reporter?.default ?? reporter;
  if (reporter?.prototype &&
      ObjectGetOwnPropertyDescriptor(reporter.prototype, 'constructor')) {
    reporter = new reporter();
  }
  if (!reporter) {
    throw new ERR_INVALID_ARG_VALUE(
      '--bench-reporter', name, 'is not a valid reporter');
  }
  return reporter;
}

async function setupReporters(stream, reporters, destinations) {
  const state = { __proto__: null, error: undefined, pending: [] };
  try {
    for (let i = 0; i < reporters.length; i++) {
      const reporter = await loadReporter(reporters[i]);
      const builtinDestination = kBuiltinDestinations.get(destinations[i]);
      const destination = builtinDestination ??
        createWriteStream(destinations[i], { __proto__: null, flush: true });
      const pending = PromisePrototypeThen(pipeline(
        stream,
        reporter,
        destination,
        { __proto__: null, end: builtinDestination === undefined },
      ), undefined, (error) => {
        state.error ??= error;
      });
      ArrayPrototypePush(state.pending, pending);
    }
  } catch (error) {
    stream.destroy();
    await SafePromiseAllReturnVoid(state.pending);
    throw error;
  }
  return state;
}

async function finishReporters(state) {
  await SafePromiseAllReturnVoid(state.pending);
  if (state.error !== undefined) throw state.error;
}

function emitRecord(stream, record) {
  switch (record.type) {
    case 'bench:plan':
      return stream.plan(record.data);
    case 'bench:start':
      return stream.start(record.data);
    case 'bench:sample':
      return stream.sample(record.data);
    case 'bench:complete':
      return stream.complete(record.data);
    case 'bench:diagnostic':
      return stream.diagnostic(record.data);
    case 'bench:summary':
      return stream.summary(record.data);
    default:
      throw new ERR_INVALID_ARG_VALUE(
        'benchmark event type', record.type, 'is not recognized');
  }
}

function emitRecordAndWait(stream, record) {
  if (stream.destroyed) {
    return PromiseReject(stream.errored ??
      new ERR_INVALID_STATE('benchmark output stream is closed'));
  }
  if (emitRecord(stream, record)) return undefined;
  if (stream.destroyed) {
    return PromiseReject(stream.errored ??
      new ERR_INVALID_STATE('benchmark output stream is closed'));
  }
  return stream.waitForDrain();
}

function serializeRecord(record) {
  if (record.data.error === undefined) return record;
  return {
    __proto__: null,
    type: record.type,
    data: {
      __proto__: null,
      ...record.data,
      error: serializeError(record.data.error),
    },
  };
}

function deserializeRecord(record) {
  if (record.data.error === undefined) return record;
  return {
    __proto__: null,
    type: record.type,
    data: {
      __proto__: null,
      ...record.data,
      error: deserializeError(record.data.error),
    },
  };
}

function isStringArray(value) {
  if (!ArrayIsArray(value)) return false;
  for (let i = 0; i < value.length; i++) {
    if (typeof value[i] !== 'string') return false;
  }
  return true;
}

function isContextDiagnostic(record) {
  return record.type === 'bench:diagnostic' &&
    (record.data.benchId !== undefined ||
     record.data.phase !== undefined ||
     record.data.index !== undefined ||
     record.data.detail !== undefined);
}

function validateRecord(record) {
  if (record === null || typeof record !== 'object' ||
      !kEventTypes.has(record.type) || record.data === null ||
      typeof record.data !== 'object' ||
      typeof record.data.runId !== 'string' ||
      (record.data.fileRunId !== null &&
       typeof record.data.fileRunId !== 'string') ||
      (record.data.entryFile !== null &&
       typeof record.data.entryFile !== 'string')) {
    throw new ERR_INVALID_ARG_VALUE(
      'benchmark child message', record, 'is not a valid benchmark record');
  }
  const contextDiagnostic = isContextDiagnostic(record);
  if ((record.type === 'bench:plan' || record.type === 'bench:start' ||
       record.type === 'bench:sample' || record.type === 'bench:complete' ||
       contextDiagnostic) &&
      (typeof record.data.fileRunId !== 'string' ||
       typeof record.data.benchId !== 'string' ||
       (record.data.parentId !== null &&
         typeof record.data.parentId !== 'string') ||
        typeof record.data.name !== 'string' ||
        !isStringArray(record.data.namePath))) {
    throw new ERR_INVALID_ARG_VALUE(
      'benchmark child message', record, 'is not a valid benchmark record');
  }
  if (record.type === 'bench:plan') {
    const {
      samples,
      selected,
      skip,
      timeout,
      warmup,
      yieldBetweenSamples,
    } = record.data;
    if (typeof record.data.file !== 'string' ||
        !NumberIsSafeInteger(record.data.line) || record.data.line < 0 ||
        !NumberIsSafeInteger(record.data.column) || record.data.column < 0 ||
        !isStringArray(record.data.tags) ||
        record.data.params === null ||
        typeof record.data.params !== 'object' ||
        ArrayIsArray(record.data.params) ||
        (ObjectGetPrototypeOf(record.data.params) !== null &&
         ObjectGetPrototypeOf(record.data.params) !== ObjectPrototype) ||
        ArrayPrototypeSome(ObjectValues(record.data.params), (value) =>
          typeof value !== 'string' && typeof value !== 'boolean' &&
          (typeof value !== 'number' || !NumberIsFinite(value))) ||
        !NumberIsSafeInteger(samples) || samples <= 0 ||
        samples > 0xFFFFFFFF ||
        !NumberIsSafeInteger(warmup) || warmup < 0 || warmup > 0xFFFFFFFF ||
        (timeout !== null &&
         (!NumberIsFinite(timeout) || timeout < 0 || timeout > TIMEOUT_MAX)) ||
        typeof yieldBetweenSamples !== 'boolean' ||
        typeof selected !== 'boolean' ||
        (selected && skip !== undefined) ||
        (!selected && skip !== true && typeof skip !== 'string')) {
      throw new ERR_INVALID_ARG_VALUE(
        'benchmark child plan', record.data,
        'is not a valid benchmark plan');
    }
  }
  if (contextDiagnostic &&
      ((record.data.phase !== 'warmup' &&
        record.data.phase !== 'measurement') ||
       !NumberIsSafeInteger(record.data.index) || record.data.index < 0 ||
       record.data.index > 0xFFFFFFFF ||
       typeof record.data.message !== 'string' ||
       (record.data.level !== 'info' && record.data.level !== 'warning') ||
       typeof record.data.file !== 'string' ||
       !NumberIsSafeInteger(record.data.line) || record.data.line < 0 ||
       !NumberIsSafeInteger(record.data.column) || record.data.column < 0 ||
       record.data.error !== undefined)) {
    throw new ERR_INVALID_ARG_VALUE(
      'benchmark child diagnostic', record.data,
      'is not a valid benchmark diagnostic');
  }
  if (record.type === 'bench:summary') {
    const { counts, duration_ns, success } = record.data;
    if (typeof success !== 'boolean' || typeof duration_ns !== 'bigint' ||
        counts === null || typeof counts !== 'object' ||
        !NumberIsSafeInteger(counts.completed) || counts.completed < 0 ||
        !NumberIsSafeInteger(counts.failed) || counts.failed < 0 ||
        !NumberIsSafeInteger(counts.skipped) || counts.skipped < 0 ||
        !NumberIsSafeInteger(counts.total) || counts.total < 0) {
      throw new ERR_INVALID_ARG_VALUE(
        'benchmark child summary', record.data,
        'is not a valid benchmark summary');
    }
  }
  return record;
}

let nextChildRecordId = 0;
let listeningForAcks = false;
const pendingChildRecordAcks = new SafeMap();

function listenForAcks() {
  if (listeningForAcks) return;
  listeningForAcks = true;
  process.on('message', (message) => {
    if (message?.type !== kChildAckMessageType) return;
    const pending = pendingChildRecordAcks.get(message.id);
    if (pending === undefined) return;
    pendingChildRecordAcks.delete(message.id);
    pending.resolve();
  });
  process.once('disconnect', () => {
    const error = new ERR_INVALID_STATE(
      'benchmark IPC channel closed before acknowledging records');
    for (const pending of pendingChildRecordAcks.values()) {
      pending.reject(error);
    }
    pendingChildRecordAcks.clear();
  });
}

function sendRecord(record) {
  listenForAcks();
  const id = nextChildRecordId++;
  const acknowledged = PromiseWithResolvers();
  pendingChildRecordAcks.set(id, acknowledged);
  try {
    process.send({
      __proto__: null,
      id,
      type: kChildMessageType,
      record: serializeRecord(record),
    }, undefined, undefined, (error) => {
      if (error) {
        pendingChildRecordAcks.delete(id);
        acknowledged.reject(error);
      }
    });
  } catch (error) {
    pendingChildRecordAcks.delete(id);
    acknowledged.reject(error);
  }
  return acknowledged.promise;
}

function sendAck(child, id) {
  return new Promise((resolve, reject) => {
    if (!child.connected) {
      reject(new ERR_INVALID_STATE(
        'benchmark child disconnected before acknowledgement'));
      return;
    }
    child.send({ __proto__: null, id, type: kChildAckMessageType },
               undefined, undefined, (error) => {
                 if (error) reject(error);
                 else resolve();
               });
  });
}

async function loadUserImports(options) {
  const parentURL = pathToFileURL(`${options.cwd}${sep}`).href;
  const loader = esmLoader.getOrInitializeCascadedLoader();
  const userImports = getOptionValue('--import');
  for (let i = 0; i < userImports.length; i++) {
    await loader.import(userImports[i], parentURL, kEmptyObject);
  }
  return { __proto__: null, loader, parentURL };
}

async function loadBenchmarkFiles(files, options, modules, onRecord) {
  const { loader, parentURL } = modules;
  let loadFailed = false;
  for (let i = 0; i < files.length; i++) {
    const file = resolve(options.cwd, files[i]);
    try {
      await runInFileScope(options.fileScopes[i], () =>
        loader.import(pathToFileURL(file), parentURL, kEmptyObject));
    } catch (error) {
      loadFailed = true;
      await onRecord({
        __proto__: null,
        type: 'bench:diagnostic',
        data: {
          __proto__: null,
          runId: options.runId,
          ...options.fileScopes[i],
          message: error?.message ?? String(error),
          error,
          level: 'error',
          file,
        },
      });
    }
  }

  const stream = runBenchmarks({
    __proto__: null,
    namePattern: options.namePattern,
    samples: options.samples,
    warmup: options.warmup,
  }, true);
  let summary;
  for await (const record of stream) {
    if (record.type === 'bench:summary' && (process.exitCode ?? 0) !== 0) {
      const scope = files.length === 1 ? options.fileScopes[0] : {
        __proto__: null,
        entryFile: null,
        fileRunId: null,
      };
      await onRecord({
        __proto__: null,
        type: 'bench:diagnostic',
        data: {
          __proto__: null,
          runId: options.runId,
          ...scope,
          message: `Benchmark process set exit code ${process.exitCode}`,
          level: 'error',
          file: files.length === 1 ?
            resolve(options.cwd, files[0]) : null,
        },
      });
    }
    if (record.type === 'bench:summary' &&
        (loadFailed || (process.exitCode ?? 0) !== 0)) {
      record.data.success = false;
    }
    if (record.type === 'bench:summary') {
      record.data.file = files.length === 1 ?
        resolve(options.cwd, files[0]) : null;
      record.data.fileRunId = files.length === 1 ?
        options.fileScopes[0].fileRunId : null;
      record.data.entryFile = files.length === 1 ?
        options.fileScopes[0].entryFile : null;
      summary = record.data;
    }
    await onRecord(record);
  }
  return summary;
}

function filterExecArgv(arg, index, args) {
  const name = getOptionName(arg);
  return !ArrayPrototypeIncludes(kFilterArgs, name) &&
    !ArrayPrototypeSome(kFilterArgValues, (option) => {
      return name === option ||
        (option !== '--experimental-config-file' &&
         index > 0 && getOptionName(args[index - 1]) === option);
    });
}

function getOptionName(arg) {
  const equals = StringPrototypeIndexOf(arg, '=');
  const name = equals === -1 ? arg : StringPrototypeSlice(arg, 0, equals);
  return StringPrototypeReplaceAll(name, '_', '-');
}

function getRunFileOptionName(arg) {
  if (StringPrototypeStartsWith(arg, '-') &&
      !StringPrototypeStartsWith(arg, '--') && arg.length > 2) {
    const shortName = StringPrototypeSlice(arg, 0, 2);
    if (kIncompatibleExecArgv.has(shortName)) return shortName;
  }
  return getOptionName(arg);
}

function filterRunFileExecArgv(arg, index, args) {
  if (!filterExecArgv(arg, index, args)) return false;
  if (!StringPrototypeStartsWith(arg, '-') || arg === '-' || arg === '--') {
    return false;
  }
  const name = getRunFileOptionName(arg);
  if (kIncompatibleExecArgv.has(name)) return false;
  if (index > 0) {
    const previous = getRunFileOptionName(args[index - 1]);
    if (kExecArgvWithValue.has(previous) &&
        StringPrototypeIndexOf(args[index - 1], '=') === -1) {
      return false;
    }
  }
  return true;
}

function runFileOptionRequiresValue(arg) {
  const equals = StringPrototypeIndexOf(arg, '=');
  let name = getRunFileOptionName(arg);
  const { aliases, options } = getCLIOptionsInfo();
  let info = options.get(name);
  if (info === undefined) {
    const alias = aliases.get(name);
    if (alias !== undefined) info = options.get(alias[0]);
  }
  if (info === undefined && StringPrototypeStartsWith(name, '--no-')) {
    name = `--${StringPrototypeSlice(name, 5)}`;
    info = options.get(name);
  }
  return info !== undefined && info.type >= 3 &&
    (equals === -1 || equals === arg.length - 1);
}

const kOptionAliases = new SafeMap([
  ['--loader', '--experimental-loader'],
  ['-C', '--conditions'],
  ['-r', '--require'],
]);

function getInheritedChildArgs() {
  const nodeOptions = getOptionsAsFlagsFromBinding();
  const args = ArrayPrototypeFilter(nodeOptions, filterExecArgv);
  const nodeOptionNames = new SafeSet();
  for (let i = 0; i < nodeOptions.length; i++) {
    nodeOptionNames.add(getOptionName(nodeOptions[i]));
  }
  const unknownExecArgv = ArrayPrototypeFilter(
    process.execArgv,
    (arg, index, values) => {
      if (!filterExecArgv(arg, index, values)) return false;
      const optionName = getOptionName(arg);
      const name = kOptionAliases.get(optionName) ?? optionName;
      if (nodeOptionNames.has(name)) return false;
      if (!StringPrototypeStartsWith(arg, '-') && index > 0) {
        const previousName = getOptionName(values[index - 1]);
        const previous = kOptionAliases.get(previousName) ?? previousName;
        if (nodeOptionNames.has(previous)) return false;
      }
      return true;
    },
  );
  ArrayPrototypePushApply(args, unknownExecArgv);
  // Option serialization omits port 0, which would otherwise become 9229.
  if (process.debugPort === 0) ArrayPrototypePush(args, '--inspect-port=0');
  return args;
}

function getChildArgs(path, options) {
  const args = options.execArgv === undefined ?
    getInheritedChildArgs() : ArrayPrototypeSlice(options.execArgv);
  ArrayPrototypePush(args, '--bench', '--bench-isolation=none');
  if (options.namePatternSource.length > 0) {
    ArrayPrototypePush(
      args, `--bench-name-pattern=${options.namePatternSource}`);
  }
  if (options.samples !== undefined) {
    ArrayPrototypePush(args, `--bench-samples=${options.samples}`);
  }
  if (options.warmup !== undefined) {
    ArrayPrototypePush(args, `--bench-warmup=${options.warmup}`);
  }
  ArrayPrototypePush(args, '--', resolve(options.cwd, path));
  return args;
}

async function runChild(path, options, scope, onRecord) {
  if (options.signal?.aborted) {
    return {
      __proto__: null,
      aborted: true,
      error: new AbortError(undefined, {
        __proto__: null,
        cause: options.signal.reason,
      }),
    };
  }
  const child = spawn(
    options.execPath ?? process.execPath,
    getChildArgs(path, options),
    {
      __proto__: null,
      cwd: options.cwd,
      env: {
        __proto__: null,
        ...(options.env ?? process.env),
        NODE_BENCH_CONTEXT: 'child',
        NODE_BENCH_FILE_RUN_ID: scope.fileRunId,
        NODE_BENCH_RUN_ID: options.runId,
      },
      serialization: 'advanced',
      stdio: ['inherit', 'pipe', 'pipe', 'ipc'],
    },
  );
  let childClosed = false;
  let forceKillTimer;
  const terminateChild = () => {
    if (childClosed) return;
    child.kill();
    forceKillTimer ??= setTimeout(() => {
      child.kill('SIGKILL');
    }, kForceKillDelay);
  };
  const closed = PromiseWithResolvers();
  let closeTracked = false;
  let spawnError;
  try {
    child.once('close', (...status) => closed.resolve(status));
    closeTracked = true;
    child.once('error', (error) => {
      spawnError ??= error;
      terminateChild();
    });
  } catch (error) {
    terminateChild();
    if (closeTracked) {
      try {
        await closed.promise;
      } catch {
        // Preserve the setup error.
      }
    }
    throw error;
  }
  let protocolError;
  let abortError;
  let aborted = false;
  let activeBenchId;
  let plansComplete = false;
  let recordPending = false;
  let summaryReceived = false;
  let abortListener;
  const closeListener = () => {
    terminateChild();
  };
  try {
    if (options.signal !== undefined) {
      abortListener = addAbortListener(options.signal, () => {
        aborted = true;
        abortError = new AbortError(undefined, {
          __proto__: null,
          cause: options.signal.reason,
        });
        terminateChild();
      });
    }
    options.output?.once('close', closeListener);
    if (options.output?.destroyed) closeListener();
  } catch (error) {
    terminateChild();
    try {
      await closed.promise;
    } catch {
      // Preserve the setup error.
    }
    childClosed = true;
    if (forceKillTimer !== undefined) clearTimeout(forceKillTimer);
    abortListener?.[SymbolDispose]();
    options.output?.removeListener('close', closeListener);
    throw error;
  }
  const pendingRecords = new SafeSet();
  const handleRecord = (record) => {
    if (protocolError !== undefined) return;
    try {
      return onRecord(record);
    } catch (error) {
      protocolError = error;
      terminateChild();
    }
  };
  const trackPending = (pending, source) => {
    if (pending === undefined) return;
    source?.pause();
    const tracked = PromisePrototypeThen(PromiseResolve(pending), () => {
      pendingRecords.delete(tracked);
      source?.resume();
    }, (error) => {
      pendingRecords.delete(tracked);
      if (!aborted) protocolError ??= error;
      terminateChild();
    });
    pendingRecords.add(tracked);
  };
  const reportOutput = (source, stream, message) => {
    const pending = handleRecord({
      __proto__: null,
      type: 'bench:diagnostic',
      data: {
        __proto__: null,
        runId: options.runId,
        ...scope,
        message,
        level: 'info',
        file: path,
        stream,
      },
    });
    trackPending(pending, source);
  };
  try {
    child.stdout.setEncoding('utf8');
    child.stderr.setEncoding('utf8');
    child.stdout.on('data', (data) => {
      reportOutput(child.stdout, 'stdout', data);
    });
    child.stderr.on('data', (data) => {
      reportOutput(child.stderr, 'stderr', data);
    });
    child.on('message', (message) => {
      if (message?.type !== kChildMessageType) return;
      try {
        if (!NumberIsSafeInteger(message.id) || message.id < 0 || recordPending) {
          throw new ERR_INVALID_ARG_VALUE(
            'benchmark child message', message,
            'does not have a valid record sequence');
        }
        recordPending = true;
        const record = deserializeRecord(validateRecord(message.record));
        const contextDiagnostic = isContextDiagnostic(record);
        if (summaryReceived || (record.type === 'bench:plan' && plansComplete) ||
            (record.type === 'bench:start' && activeBenchId !== undefined) ||
            ((record.type === 'bench:sample' || contextDiagnostic) &&
             activeBenchId !== record.data.benchId) ||
            (record.type === 'bench:complete' && activeBenchId !== undefined &&
             activeBenchId !== record.data.benchId) ||
            (record.type === 'bench:summary' && activeBenchId !== undefined)) {
          throw new ERR_INVALID_ARG_VALUE(
            'benchmark child message', message,
            'does not have a valid lifecycle sequence');
        }
        if (record.type !== 'bench:plan' &&
            record.type !== 'bench:diagnostic') {
          plansComplete = true;
        }
        if (record.type === 'bench:start') {
          activeBenchId = record.data.benchId;
        } else if (record.type === 'bench:complete' &&
                   activeBenchId === record.data.benchId) {
          activeBenchId = undefined;
        }
        if (record.type === 'bench:summary') summaryReceived = true;
        record.data.runId = options.runId;
        record.data.fileRunId = scope.fileRunId;
        if (record.data.entryFile !== null) {
          record.data.entryFile = scope.entryFile;
        }
        const pending = handleRecord(record);
        if (protocolError === undefined) {
          const acknowledged = PromisePrototypeThen(
            PromiseResolve(pending), () => sendAck(child, message.id));
          trackPending(PromisePrototypeThen(acknowledged, () => {
            recordPending = false;
          }));
        }
      } catch (error) {
        if (!aborted) protocolError ??= error;
        terminateChild();
      }
    });
  } catch (error) {
    terminateChild();
    try {
      await closed.promise;
    } catch {
      // Preserve the setup error.
    }
    childClosed = true;
    if (forceKillTimer !== undefined) clearTimeout(forceKillTimer);
    abortListener?.[SymbolDispose]();
    options.output?.removeListener('close', closeListener);
    throw error;
  }
  let status;
  try {
    status = await closed.promise;
  } finally {
    childClosed = true;
    if (forceKillTimer !== undefined) clearTimeout(forceKillTimer);
    abortListener?.[SymbolDispose]();
    options.output?.removeListener('close', closeListener);
  }
  const { 0: code, 1: signal } = status;
  await SafePromiseAllReturnVoid(ArrayFrom(pendingRecords));
  if (!aborted && spawnError !== undefined) throw spawnError;
  if (!aborted && protocolError !== undefined) throw protocolError;
  return {
    __proto__: null,
    aborted,
    code,
    error: abortError,
    signal,
  };
}

async function runIsolated(files, options, output) {
  const counts = {
    __proto__: null,
    completed: 0,
    failed: 0,
    skipped: 0,
    total: 0,
  };
  const start = hrtime();
  let success = true;

  for (let i = 0; i < files.length; i++) {
    const path = files[i];
    const scope = options.fileScopes[i];
    const observedCounts = {
      __proto__: null,
      completed: 0,
      failed: 0,
      skipped: 0,
      total: 0,
    };
    let childSummary;
    let result;
    try {
      result = await runChild(path, options, scope, (record) => {
        if (record.type === 'bench:summary') {
          childSummary = record.data;
          return;
        }
        if (record.type === 'bench:plan') observedCounts.total++;
        if (record.type === 'bench:complete') {
          if (ObjectPrototypeHasOwnProperty(record.data, 'error')) {
            observedCounts.failed++;
          } else if (ObjectPrototypeHasOwnProperty(record.data, 'skip')) {
            observedCounts.skipped++;
          } else {
            observedCounts.completed++;
          }
        }
        return emitRecordAndWait(output, record);
      });
    } catch (error) {
      success = false;
      await emitRecordAndWait(output, {
        __proto__: null,
        type: 'bench:diagnostic',
        data: {
          __proto__: null,
          runId: options.runId,
          ...scope,
          message: error.message,
          error,
          level: 'error',
          file: path,
        },
      });
      counts.completed += observedCounts.completed;
      counts.failed += observedCounts.failed;
      counts.skipped += observedCounts.skipped;
      counts.total += MathMax(
        observedCounts.total,
        observedCounts.completed + observedCounts.failed +
          observedCounts.skipped);
      continue;
    }

    if (result.aborted) {
      success = false;
      await emitRecordAndWait(output, {
        __proto__: null,
        type: 'bench:diagnostic',
        data: {
          __proto__: null,
          runId: options.runId,
          ...scope,
          message: result.error.message,
          error: result.error,
          level: 'error',
          file: path,
        },
      });
    }
    if (childSummary !== undefined) {
      success &&= childSummary.success;
      counts.completed += childSummary.counts.completed;
      counts.failed += childSummary.counts.failed;
      counts.skipped += childSummary.counts.skipped;
      counts.total += childSummary.counts.total;
    } else {
      counts.completed += observedCounts.completed;
      counts.failed += observedCounts.failed;
      counts.skipped += observedCounts.skipped;
      counts.total += MathMax(
        observedCounts.total,
        observedCounts.completed + observedCounts.failed +
          observedCounts.skipped);
    }
    if (!result.aborted && (childSummary === undefined || result.code !== 0 ||
        result.signal !== null)) {
      success = false;
      if (childSummary === undefined || childSummary.success) {
        const status = result.signal === null ?
          `exit code ${result.code}` : `signal ${result.signal}`;
        await emitRecordAndWait(output, {
          __proto__: null,
          type: 'bench:diagnostic',
          data: {
            __proto__: null,
            runId: options.runId,
            ...scope,
            message: `Benchmark file '${path}' failed with ${status}`,
            level: 'error',
            file: path,
          },
        });
      }
    }
  }

  const scope = files.length === 1 ? options.fileScopes[0] : null;
  const summary = {
    __proto__: null,
    runId: options.runId,
    fileRunId: scope?.fileRunId ?? null,
    entryFile: scope?.entryFile ?? null,
    success: success && (options.useProcessExitCode === false ||
      (process.exitCode ?? 0) === 0),
    counts,
    duration_ns: hrtime() - start,
    file: files.length === 1 ? resolve(options.cwd, files[0]) : null,
  };
  await emitRecordAndWait(output, {
    __proto__: null,
    type: 'bench:summary',
    data: summary,
  });
  return summary;
}

function runFile(path, options = kEmptyObject) {
  validateStringWithoutNullBytes(path, 'path');
  if (!isAbsolute(path)) {
    throw new ERR_INVALID_ARG_VALUE('path', path, 'must be an absolute path');
  }
  const file = resolve(path);
  validateObject(options, 'options');
  const {
    env = process.env,
    execArgv,
    signal,
  } = options;
  let childExecArgv;
  if (execArgv !== undefined) {
    validateArray(execArgv, 'options.execArgv');
    childExecArgv = [];
    const length = execArgv.length;
    for (let i = 0; i < length; i++) {
      const arg = execArgv[i];
      validateStringWithoutNullBytes(arg, `options.execArgv[${i}]`);
      if (!StringPrototypeStartsWith(arg, '-') || arg === '-' || arg === '--') {
        throw new ERR_INVALID_ARG_VALUE(
          `options.execArgv[${i}]`, arg,
          'must be a Node.js command-line option');
      }
      if (kIncompatibleExecArgv.has(getRunFileOptionName(arg))) {
        throw new ERR_INVALID_ARG_VALUE(
          `options.execArgv[${i}]`, arg,
          'is not compatible with benchmark execution');
      }
      if (runFileOptionRequiresValue(arg)) {
        const equals = StringPrototypeIndexOf(arg, '=');
        if (equals !== -1) {
          throw new ERR_INVALID_ARG_VALUE(
            `options.execArgv[${i}]`, arg,
            'must include a non-empty value');
        }
        if (i + 1 >= length) {
          throw new ERR_INVALID_ARG_VALUE(
            `options.execArgv[${i}]`, arg,
            'must be followed by a value');
        }
        ArrayPrototypePush(childExecArgv, arg);
        i++;
        const value = execArgv[i];
        validateStringWithoutNullBytes(value, `options.execArgv[${i}]`);
        if (value.length === 0) {
          throw new ERR_INVALID_ARG_VALUE(
            `options.execArgv[${i}]`, value,
            'must not be empty');
        }
        ArrayPrototypePush(childExecArgv, value);
        continue;
      }
      if (!StringPrototypeStartsWith(arg, '--') &&
          StringPrototypeIndexOf(arg, '=') !== -1) {
        throw new ERR_INVALID_ARG_VALUE(
          `options.execArgv[${i}]`, arg,
          'must not use = with a short option');
      }
      ArrayPrototypePush(childExecArgv, arg);
    }
    if (ArrayPrototypeFilter(childExecArgv, filterExecArgv).length !==
        childExecArgv.length) {
      throw new ERR_INVALID_ARG_VALUE(
        'options.execArgv', childExecArgv,
        'must not contain benchmark runner options');
    }
  } else {
    childExecArgv = ArrayPrototypeFilter(
      getInheritedChildArgs(), filterRunFileExecArgv);
  }
  validateObject(env, 'options.env');
  const childEnv = { __proto__: null };
  const envKeys = ObjectKeys(env);
  for (let i = 0; i < envKeys.length; i++) {
    const key = envKeys[i];
    validateStringWithoutNullBytes(key, 'options.env key');
    const value = env[key];
    if (value === undefined) continue;
    validateStringWithoutNullBytes(value, `options.env.${key}`);
    if (kIPCEnvironmentVariables.has(StringPrototypeToUpperCase(key))) continue;
    childEnv[key] = value;
  }
  validateAbortSignal(signal, 'options.signal');
  if (signal !== undefined &&
      (typeof signal.addEventListener !== 'function' ||
       typeof signal.removeEventListener !== 'function')) {
    throw new ERR_INVALID_ARG_TYPE('options.signal', 'AbortSignal', signal);
  }

  const runId = createRunId();
  const scope = {
    __proto__: null,
    entryFile: file,
    fileRunId: createRunId(),
  };
  const output = new BenchmarksStream();
  const runOptions = {
    __proto__: null,
    cwd: process.cwd(),
    env: childEnv,
    execPath: process.execPath,
    execArgv: childExecArgv,
    fileScopes: [scope],
    namePatternSource: '',
    output,
    runId,
    signal,
    useProcessExitCode: false,
  };
  const execution = PromisePrototypeThen(PromiseResolve(), () =>
    (output.destroyed ? undefined : runIsolated([file], runOptions, output)));
  PromisePrototypeThen(execution, () => output.end(),
                       (error) => output.destroy(error));
  return output;
}

async function run(patterns) {
  const options = parseCommandLine();
  const files = options.isChild ?
    createChildFileList(patterns, options.cwd) :
    createBenchmarkFileList(patterns, options.cwd);
  if (files === null) return { __proto__: null, success: false };

  options.runId ??= createRunId();
  options.fileScopes = createFileScopes(files, options);
  const scope = files.length === 1 ? options.fileScopes[0] : {
    __proto__: null,
    entryFile: null,
    fileRunId: options.runId,
  };
  configureRunScope({
    __proto__: null,
    runId: options.runId,
    ...scope,
  });

  if (options.isChild) {
    try {
      const modules = await loadUserImports(options);
      return await loadBenchmarkFiles(
        files, options, modules, sendRecord);
    } finally {
      if (process.connected) process.disconnect();
    }
  }

  const output = new BenchmarksStream();
  let executionError;
  let reporterState;
  let summary;
  try {
    options.modules = await loadUserImports(options);
    reporterState = await setupReporters(
      output, options.reporters, options.destinations);
    if (options.isolation === 'process') {
      summary = await runIsolated(files, options, output);
    } else {
      summary = await loadBenchmarkFiles(
        files,
        options,
        options.modules,
        (record) => emitRecordAndWait(output, record),
      );
    }
  } catch (error) {
    executionError = error;
  } finally {
    output.end();
  }
  if (reporterState !== undefined) await finishReporters(reporterState);
  if (executionError !== undefined) throw executionError;
  return summary;
}

module.exports = { run, runFile };
