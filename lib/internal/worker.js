'use strict';

const {
  ArrayIsArray,
  ArrayPrototypeForEach,
  ArrayPrototypeMap,
  ArrayPrototypePush,
  Float64Array,
  FunctionPrototypeBind,
  JSONStringify,
  MathMax,
  ObjectCreate,
  ObjectEntries,
  Promise,
  PromiseResolve,
  ReflectApply,
  RegExpPrototypeTest,
  SafeArrayIterator,
  SafeMap,
  String,
  Symbol,
  SymbolFor,
  TypedArrayPrototypeFill,
  Uint32Array,
  globalThis: { Atomics, SharedArrayBuffer },
} = primordials;

const EventEmitter = require('events');
const assert = require('internal/assert');
const path = require('path');
const { now } = require('internal/perf/utils');

const errorCodes = require('internal/errors').codes;
const {
  ERR_WORKER_NOT_RUNNING,
  ERR_WORKER_PATH,
  ERR_WORKER_UNSERIALIZABLE_ERROR,
  ERR_WORKER_UNSUPPORTED_EXTENSION,
  ERR_WORKER_INVALID_EXEC_ARGV,
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_ARG_VALUE,
} = errorCodes;
const { getOptionValue } = require('internal/options');

const workerIo = require('internal/worker/io');
const {
  drainMessagePort,
  MessageChannel,
  messageTypes,
  kPort,
  kIncrementsPortRef,
  kWaitingStreams,
  kStdioWantsMoreDataCallback,
  setupPortReferencing,
  ReadableWorkerStdio,
  WritableWorkerStdio
} = workerIo;
const { deserializeError } = require('internal/error_serdes');
const { fileURLToPath, isURLInstance, pathToFileURL } = require('internal/url');
const { validateArray } = require('internal/validators');

const {
  ownsProcessState,
  isMainThread,
  resourceLimits: resourceLimitsRaw,
  threadId,
  Worker: WorkerImpl,
  kMaxYoungGenerationSizeMb,
  kMaxOldGenerationSizeMb,
  kCodeRangeSizeMb,
  kStackSizeMb,
  kTotalResourceLimitCount
} = internalBinding('worker');

const kHandle = Symbol('kHandle');
const kPublicPort = Symbol('kPublicPort');
const kDispose = Symbol('kDispose');
const kOnExit = Symbol('kOnExit');
const kOnMessage = Symbol('kOnMessage');
const kOnCouldNotSerializeErr = Symbol('kOnCouldNotSerializeErr');
const kOnErrorMessage = Symbol('kOnErrorMessage');
const kParentSideStdio = Symbol('kParentSideStdio');
const kLoopStartTime = Symbol('kLoopStartTime');
const kIsOnline = Symbol('kIsOnline');

const SHARE_ENV = SymbolFor('nodejs.worker_threads.SHARE_ENV');
let debug = require('internal/util/debuglog').debuglog('worker', (fn) => {
  debug = fn;
});

let cwdCounter;

const environmentData = new SafeMap();

if (isMainThread) {
  cwdCounter = new Uint32Array(new SharedArrayBuffer(4));
  const originalChdir = process.chdir;
  process.chdir = function(path) {
    Atomics.add(cwdCounter, 0, 1);
    originalChdir(path);
  };
}

function setEnvironmentData(key, value) {
  if (value === undefined)
    environmentData.delete(key);
  else
    environmentData.set(key, value);
}

function getEnvironmentData(key) {
  return environmentData.get(key);
}

function assignEnvironmentData(data) {
  if (data === undefined) return;
  data.forEach((value, key) => {
    environmentData.set(key, value);
  });
}

class Worker extends EventEmitter {
  constructor(filename, options = {}) {
    super();
    debug(`[${threadId}] create new worker`, filename, options);
    if (options.execArgv && !ArrayIsArray(options.execArgv)) {
      throw new ERR_INVALID_ARG_TYPE('options.execArgv',
                                     'Array',
                                     options.execArgv);
    }
    let argv;
    if (options.argv) {
      validateArray(options.argv, 'options.argv');
      argv = ArrayPrototypeMap(options.argv, String);
    }

    let url, doEval;
    if (options.eval) {
      if (typeof filename !== 'string') {
        throw new ERR_INVALID_ARG_VALUE(
          'options.eval',
          options.eval,
          'must be false when \'filename\' is not a string'
        );
      }
      url = null;
      doEval = 'classic';
    } else if (isURLInstance(filename) && filename.protocol === 'data:') {
      url = null;
      doEval = 'module';
      filename = `import ${JSONStringify(`${filename}`)}`;
    } else {
      doEval = false;
      if (isURLInstance(filename)) {
        url = filename;
        filename = fileURLToPath(filename);
      } else if (typeof filename !== 'string') {
        throw new ERR_INVALID_ARG_TYPE(
          'filename',
          ['string', 'URL'],
          filename
        );
      } else if (path.isAbsolute(filename) ||
                 RegExpPrototypeTest(/^\.\.?[\\/]/, filename)) {
        filename = path.resolve(filename);
        url = pathToFileURL(filename);
      } else {
        throw new ERR_WORKER_PATH(filename);
      }

      const ext = path.extname(filename);
      if (ext !== '.js' && ext !== '.mjs' && ext !== '.cjs') {
        throw new ERR_WORKER_UNSUPPORTED_EXTENSION(ext);
      }
    }

    let env;
    if (typeof options.env === 'object' && options.env !== null) {
      env = ObjectCreate(null);
      ArrayPrototypeForEach(
        ObjectEntries(options.env),
        ({ 0: key, 1: value }) => { env[key] = `${value}`; }
      );
    } else if (options.env == null) {
      env = process.env;
    } else if (options.env !== SHARE_ENV) {
      throw new ERR_INVALID_ARG_TYPE(
        'options.env',
        ['object', 'undefined', 'null', 'worker_threads.SHARE_ENV'],
        options.env);
    }

    // Set up the C++ handle for the worker, as well as some internal wiring.
    this[kHandle] = new WorkerImpl(url,
                                   env === process.env ? null : env,
                                   options.execArgv,
                                   parseResourceLimits(options.resourceLimits),
                                   !!(options.trackUnmanagedFds ?? true));
    if (this[kHandle].invalidExecArgv) {
      throw new ERR_WORKER_INVALID_EXEC_ARGV(this[kHandle].invalidExecArgv);
    }
    if (this[kHandle].invalidNodeOptions) {
      throw new ERR_WORKER_INVALID_EXEC_ARGV(
        this[kHandle].invalidNodeOptions, 'invalid NODE_OPTIONS env variable');
    }
    this[kHandle].onexit = (code, customErr, customErrReason) => {
      this[kOnExit](code, customErr, customErrReason);
    };
    this[kPort] = this[kHandle].messagePort;
    this[kPort].on('message', (data) => this[kOnMessage](data));
    this[kPort].start();
    this[kPort].unref();
    this[kPort][kWaitingStreams] = 0;
    debug(`[${threadId}] created Worker with ID ${this.threadId}`);

    let stdin = null;
    if (options.stdin)
      stdin = new WritableWorkerStdio(this[kPort], 'stdin');
    const stdout = new ReadableWorkerStdio(this[kPort], 'stdout');
    if (!options.stdout) {
      stdout[kIncrementsPortRef] = false;
      pipeWithoutWarning(stdout, process.stdout);
    }
    const stderr = new ReadableWorkerStdio(this[kPort], 'stderr');
    if (!options.stderr) {
      stderr[kIncrementsPortRef] = false;
      pipeWithoutWarning(stderr, process.stderr);
    }

    this[kParentSideStdio] = { stdin, stdout, stderr };

    const { port1, port2 } = new MessageChannel();
    const transferList = [port2];
    // If transferList is provided.
    if (options.transferList)
      ArrayPrototypePush(transferList,
                         ...new SafeArrayIterator(options.transferList));

    this[kPublicPort] = port1;
    ArrayPrototypeForEach(['message', 'messageerror'], (event) => {
      this[kPublicPort].on(event, (message) => this.emit(event, message));
    });
    setupPortReferencing(this[kPublicPort], this, 'message');
    this[kPort].postMessage({
      argv,
      type: messageTypes.LOAD_SCRIPT,
      filename,
      doEval,
      cwdCounter: cwdCounter || workerIo.sharedCwdCounter,
      workerData: options.workerData,
      environmentData,
      publicPort: port2,
      manifestURL: getOptionValue('--experimental-policy') ?
        require('internal/process/policy').url :
        null,
      manifestSrc: getOptionValue('--experimental-policy') ?
        require('internal/process/policy').src :
        null,
      hasStdin: !!options.stdin
    }, transferList);
    // Use this to cache the Worker's loopStart value once available.
    this[kLoopStartTime] = -1;
    this[kIsOnline] = false;
    this.performance = {
      eventLoopUtilization: FunctionPrototypeBind(eventLoopUtilization, this),
    };
    // Actually start the new thread now that everything is in place.
    this[kHandle].startThread();

    process.nextTick(() => process.emit('worker', this));
  }

  [kOnExit](code, customErr, customErrReason) {
    debug(`[${threadId}] hears end event for Worker ${this.threadId}`);
    drainMessagePort(this[kPublicPort]);
    drainMessagePort(this[kPort]);
    this.removeAllListeners('message');
    this.removeAllListeners('messageerrors');
    this[kPublicPort].unref();
    this[kPort].unref();
    this[kDispose]();
    if (customErr) {
      debug(`[${threadId}] failing with custom error ${customErr} \
        and with reason ${customErrReason}`);
      this.emit('error', new errorCodes[customErr](customErrReason));
    }
    this.emit('exit', code);
    this.removeAllListeners();
  }

  [kOnCouldNotSerializeErr]() {
    this.emit('error', new ERR_WORKER_UNSERIALIZABLE_ERROR());
  }

  [kOnErrorMessage](serialized) {
    // This is what is called for uncaught exceptions.
    const error = deserializeError(serialized);
    this.emit('error', error);
  }

  [kOnMessage](message) {
    switch (message.type) {
      case messageTypes.UP_AND_RUNNING:
        this[kIsOnline] = true;
        return this.emit('online');
      case messageTypes.COULD_NOT_SERIALIZE_ERROR:
        return this[kOnCouldNotSerializeErr]();
      case messageTypes.ERROR_MESSAGE:
        return this[kOnErrorMessage](message.error);
      case messageTypes.STDIO_PAYLOAD:
      {
        const { stream, chunks } = message;
        const readable = this[kParentSideStdio][stream];
        ArrayPrototypeForEach(chunks, ({ chunk, encoding }) => {
          readable.push(chunk, encoding);
        });
        return;
      }
      case messageTypes.STDIO_WANTS_MORE_DATA:
      {
        const { stream } = message;
        return this[kParentSideStdio][stream][kStdioWantsMoreDataCallback]();
      }
    }

    assert.fail(`Unknown worker message type ${message.type}`);
  }

  [kDispose]() {
    this[kHandle].onexit = null;
    this[kHandle] = null;
    this[kPort] = null;
    this[kPublicPort] = null;

    const { stdout, stderr } = this[kParentSideStdio];

    if (!stdout.readableEnded) {
      debug(`[${threadId}] explicitly closes stdout for ${this.threadId}`);
      stdout.push(null);
    }
    if (!stderr.readableEnded) {
      debug(`[${threadId}] explicitly closes stderr for ${this.threadId}`);
      stderr.push(null);
    }
  }

  postMessage(...args) {
    if (this[kPublicPort] === null) return;

    ReflectApply(this[kPublicPort].postMessage, this[kPublicPort], args);
  }

  terminate(callback) {
    debug(`[${threadId}] terminates Worker with ID ${this.threadId}`);

    this.ref();

    if (typeof callback === 'function') {
      process.emitWarning(
        'Passing a callback to worker.terminate() is deprecated. ' +
        'It returns a Promise instead.',
        'DeprecationWarning', 'DEP0132');
      if (this[kHandle] === null) return PromiseResolve();
      this.once('exit', (exitCode) => callback(null, exitCode));
    }

    if (this[kHandle] === null) return PromiseResolve();

    this[kHandle].stopThread();

    // Do not use events.once() here, because the 'exit' event will always be
    // emitted regardless of any errors, and the point is to only resolve
    // once the thread has actually stopped.
    return new Promise((resolve) => {
      this.once('exit', resolve);
    });
  }

  ref() {
    if (this[kHandle] === null) return;

    this[kHandle].ref();
    this[kPublicPort].ref();
  }

  unref() {
    if (this[kHandle] === null) return;

    this[kHandle].unref();
    this[kPublicPort].unref();
  }

  get threadId() {
    if (this[kHandle] === null) return -1;

    return this[kHandle].threadId;
  }

  get stdin() {
    return this[kParentSideStdio].stdin;
  }

  get stdout() {
    return this[kParentSideStdio].stdout;
  }

  get stderr() {
    return this[kParentSideStdio].stderr;
  }

  get resourceLimits() {
    if (this[kHandle] === null) return {};

    return makeResourceLimits(this[kHandle].getResourceLimits());
  }

  getHeapSnapshot() {
    const heapSnapshotTaker = this[kHandle] && this[kHandle].takeHeapSnapshot();
    return new Promise((resolve, reject) => {
      if (!heapSnapshotTaker) return reject(new ERR_WORKER_NOT_RUNNING());
      heapSnapshotTaker.ondone = (handle) => {
        const { HeapSnapshotStream } = require('internal/heap_utils');
        resolve(new HeapSnapshotStream(handle));
      };
    });
  }
}

function pipeWithoutWarning(source, dest) {
  const sourceMaxListeners = source._maxListeners;
  const destMaxListeners = dest._maxListeners;
  source.setMaxListeners(Infinity);
  dest.setMaxListeners(Infinity);

  source.pipe(dest);

  source._maxListeners = sourceMaxListeners;
  dest._maxListeners = destMaxListeners;
}

const resourceLimitsArray = new Float64Array(kTotalResourceLimitCount);
function parseResourceLimits(obj) {
  const ret = resourceLimitsArray;
  TypedArrayPrototypeFill(ret, -1);
  if (typeof obj !== 'object' || obj === null) return ret;

  if (typeof obj.maxOldGenerationSizeMb === 'number')
    ret[kMaxOldGenerationSizeMb] = MathMax(obj.maxOldGenerationSizeMb, 2);
  if (typeof obj.maxYoungGenerationSizeMb === 'number')
    ret[kMaxYoungGenerationSizeMb] = obj.maxYoungGenerationSizeMb;
  if (typeof obj.codeRangeSizeMb === 'number')
    ret[kCodeRangeSizeMb] = obj.codeRangeSizeMb;
  if (typeof obj.stackSizeMb === 'number')
    ret[kStackSizeMb] = obj.stackSizeMb;
  return ret;
}

function makeResourceLimits(float64arr) {
  return {
    maxYoungGenerationSizeMb: float64arr[kMaxYoungGenerationSizeMb],
    maxOldGenerationSizeMb: float64arr[kMaxOldGenerationSizeMb],
    codeRangeSizeMb: float64arr[kCodeRangeSizeMb],
    stackSizeMb: float64arr[kStackSizeMb]
  };
}

function eventLoopUtilization(util1, util2) {
  // TODO(trevnorris): Works to solve the thread-safe read/write issue of
  // loopTime, but has the drawback that it can't be set until the event loop
  // has had a chance to turn. So it will be impossible to read the ELU of
  // a worker thread immediately after it's been created.
  if (!this[kIsOnline] || !this[kHandle]) {
    return { idle: 0, active: 0, utilization: 0 };
  }

  // Cache loopStart, since it's only written to once.
  if (this[kLoopStartTime] === -1) {
    this[kLoopStartTime] = this[kHandle].loopStartTime();
    if (this[kLoopStartTime] === -1)
      return { idle: 0, active: 0, utilization: 0 };
  }

  if (util2) {
    const idle = util1.idle - util2.idle;
    const active = util1.active - util2.active;
    return { idle, active, utilization: active / (idle + active) };
  }

  const idle = this[kHandle].loopIdleTime();

  // Using performance.now() here is fine since it's always the time from
  // the beginning of the process, and is why it needs to be offset by the
  // loopStart time (which is also calculated from the beginning of the
  // process).
  const active = now() - this[kLoopStartTime] - idle;

  if (!util1) {
    return { idle, active, utilization: active / (idle + active) };
  }

  const idle_delta = idle - util1.idle;
  const active_delta = active - util1.active;
  const utilization = active_delta / (idle_delta + active_delta);
  return { idle: idle_delta, active: active_delta, utilization };
}

module.exports = {
  ownsProcessState,
  isMainThread,
  SHARE_ENV,
  resourceLimits:
    !isMainThread ? makeResourceLimits(resourceLimitsRaw) : {},
  setEnvironmentData,
  getEnvironmentData,
  assignEnvironmentData,
  threadId,
  Worker,
};
