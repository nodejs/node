'use strict';

const {
  ArrayFrom,
  ArrayPrototypeFilter,
  ArrayPrototypeIncludes,
  ArrayPrototypeJoin,
  ArrayPrototypePush,
  ArrayPrototypePushApply,
  ArrayPrototypeSome,
  ArrayPrototypeSort,
  NumberIsSafeInteger,
  ObjectGetOwnPropertyDescriptor,
  Promise,
  PromisePrototypeThen,
  PromiseReject,
  PromiseResolve,
  RegExp,
  SafeMap,
  SafePromiseAllReturnVoid,
  SafeSet,
  String,
  StringPrototypeIndexOf,
  StringPrototypeSlice,
  StringPrototypeStartsWith,
} = primordials;
const { spawn } = require('child_process');
const { createWriteStream, statSync } = require('fs');
const { Glob } = require('internal/fs/glob');
const {
  BenchmarksStream,
  kBenchmarksStreamDrain,
} = require('internal/bench_runner/benchmarks_stream');
const {
  runBenchmarks,
} = require('internal/bench_runner/harness');
const { deserializeError, serializeError } = require('internal/error_serdes');
const {
  codes: {
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_STATE,
  },
} = require('internal/errors');
const { getOptionValue, getOptionsAsFlagsFromBinding } = require('internal/options');
const { kEmptyObject } = require('internal/util');
const { validateUint32 } = require('internal/validators');
const { pathToFileURL } = require('internal/url');
const { pipeline } = require('stream/promises');
const { once } = require('events');
const { resolve, sep } = require('path');

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
const kChildMessageType = 'node:bench:record';
const kEventTypes = new SafeSet([
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
    isChild,
    isolation: getOptionValue('--bench-isolation'),
    namePattern: parseNamePattern(getOptionValue('--bench-name-pattern')),
    namePatternSource: getOptionValue('--bench-name-pattern'),
    reporters,
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
  return once(stream, kBenchmarksStreamDrain);
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

function validateRecord(record) {
  if (record === null || typeof record !== 'object' ||
      !kEventTypes.has(record.type) || record.data === null ||
      typeof record.data !== 'object') {
    throw new ERR_INVALID_ARG_VALUE(
      'benchmark child message', record, 'is not a valid benchmark record');
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

function sendRecord(record) {
  return new Promise((resolve, reject) => {
    try {
      process.send({
        __proto__: null,
        type: kChildMessageType,
        record: serializeRecord(record),
      }, undefined, undefined, (error) => {
        if (error) reject(error);
        else resolve();
      });
    } catch (error) {
      reject(error);
    }
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
      await loader.import(pathToFileURL(file), parentURL, kEmptyObject);
    } catch (error) {
      loadFailed = true;
      await onRecord({
        __proto__: null,
        type: 'bench:diagnostic',
        data: {
          __proto__: null,
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
    if (record.type === 'bench:summary' &&
        (loadFailed || (process.exitCode ?? 0) !== 0)) {
      record.data.success = false;
    }
    if (record.type === 'bench:summary') {
      record.data.file = files.length === 1 ?
        resolve(options.cwd, files[0]) : null;
      summary = record.data;
    }
    await onRecord(record);
  }
  return summary;
}

function filterExecArgv(arg, index, args) {
  return !ArrayPrototypeIncludes(kFilterArgs, arg) &&
    !ArrayPrototypeSome(kFilterArgValues, (option) => {
      return arg === option ||
        StringPrototypeStartsWith(arg, `${option}=`) ||
        (option !== '--experimental-config-file' &&
         index > 0 && args[index - 1] === option);
    });
}

function getOptionName(arg) {
  const equals = StringPrototypeIndexOf(arg, '=');
  return equals === -1 ? arg : StringPrototypeSlice(arg, 0, equals);
}

const kOptionAliases = new SafeMap([
  ['--loader', '--experimental-loader'],
  ['-C', '--conditions'],
  ['-r', '--require'],
]);

function getChildArgs(path, options) {
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

async function runChild(path, options, onRecord) {
  const child = spawn(process.execPath, getChildArgs(path, options), {
    __proto__: null,
    cwd: options.cwd,
    env: {
      __proto__: null,
      ...process.env,
      NODE_BENCH_CONTEXT: 'child',
    },
    serialization: 'advanced',
    stdio: ['inherit', 'pipe', 'pipe', 'ipc'],
  });
  let protocolError;
  const pendingRecords = new SafeSet();
  const handleRecord = (record) => {
    if (protocolError !== undefined) return;
    try {
      return onRecord(record);
    } catch (error) {
      protocolError = error;
      child.kill();
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
      protocolError = error;
      child.kill();
    });
    pendingRecords.add(tracked);
  };
  const reportOutput = (source, stream, message) => {
    const pending = handleRecord({
      __proto__: null,
      type: 'bench:diagnostic',
      data: {
        __proto__: null,
        message,
        level: 'info',
        file: path,
        stream,
      },
    });
    trackPending(pending, source);
  };
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
      const pending = handleRecord(
        deserializeRecord(validateRecord(message.record)));
      trackPending(pending);
    } catch (error) {
      protocolError = error;
      child.kill();
    }
  });
  const { 0: code, 1: signal } = await once(child, 'close');
  await SafePromiseAllReturnVoid(ArrayFrom(pendingRecords));
  if (protocolError !== undefined) throw protocolError;
  return { __proto__: null, code, signal };
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
    let childSummary;
    let result;
    try {
      result = await runChild(path, options, (record) => {
        if (record.type === 'bench:summary') {
          childSummary = record.data;
          return;
        }
        return emitRecordAndWait(output, record);
      });
    } catch (error) {
      success = false;
      output.diagnostic({
        __proto__: null,
        message: error.message,
        error,
        level: 'error',
        file: path,
      });
      continue;
    }

    if (childSummary !== undefined) {
      success &&= childSummary.success;
      counts.completed += childSummary.counts.completed;
      counts.failed += childSummary.counts.failed;
      counts.skipped += childSummary.counts.skipped;
      counts.total += childSummary.counts.total;
    }
    if (childSummary === undefined || result.code !== 0 ||
        result.signal !== null) {
      success = false;
      if (childSummary === undefined || childSummary.success) {
        const status = result.signal === null ?
          `exit code ${result.code}` : `signal ${result.signal}`;
        output.diagnostic({
          __proto__: null,
          message: `Benchmark file '${path}' failed with ${status}`,
          level: 'error',
          file: path,
        });
      }
    }
  }

  const summary = {
    __proto__: null,
    success: success && (process.exitCode ?? 0) === 0,
    counts,
    duration_ns: hrtime() - start,
    file: files.length === 1 ? resolve(options.cwd, files[0]) : null,
  };
  output.summary(summary);
  return summary;
}

async function run(patterns) {
  const options = parseCommandLine();
  const files = options.isChild ?
    createChildFileList(patterns, options.cwd) :
    createBenchmarkFileList(patterns, options.cwd);
  if (files === null) return { __proto__: null, success: false };

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

module.exports = { run };
