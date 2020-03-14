'use strict';

/* global SharedArrayBuffer */

const {
  ArrayIsArray,
  MathMax,
  ObjectCreate,
  ObjectEntries,
  Promise,
  PromiseResolve,
  Symbol,
  SymbolFor,
} = primordials;

const EventEmitter = require('events');
const assert = require('internal/assert');
const path = require('path');

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
const { deserializeError } = require('internal/error-serdes');
const { fileURLToPath, isURLInstance, pathToFileURL } = require('internal/url');

const {
  ownsProcessState,
  isMainThread,
  resourceLimits: resourceLimitsRaw,
  threadId,
  Worker: WorkerImpl,
  kMaxYoungGenerationSizeMb,
  kMaxOldGenerationSizeMb,
  kCodeRangeSizeMb,
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

const SHARE_ENV = SymbolFor('nodejs.worker_threads.SHARE_ENV');
const debug = require('internal/util/debuglog').debuglog('worker');

let cwdCounter;

if (isMainThread) {
  cwdCounter = new Uint32Array(new SharedArrayBuffer(4));
  const originalChdir = process.chdir;
  process.chdir = function(path) {
    Atomics.add(cwdCounter, 0, 1);
    originalChdir(path);
  };
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
      if (!ArrayIsArray(options.argv)) {
        throw new ERR_INVALID_ARG_TYPE('options.argv', 'Array', options.argv);
      }
      argv = options.argv.map(String);
    }

    let url;
    if (options.eval) {
      if (typeof filename !== 'string') {
        throw new ERR_INVALID_ARG_VALUE(
          'options.eval',
          options.eval,
          'must be false when \'filename\' is not a string'
        );
      }
      url = null;
    } else {
      if (isURLInstance(filename)) {
        url = filename;
        filename = fileURLToPath(filename);
      } else if (typeof filename !== 'string') {
        throw new ERR_INVALID_ARG_TYPE(
          'filename',
          ['string', 'URL'],
          filename
        );
      } else if (path.isAbsolute(filename) || /^\.\.?[\\/]/.test(filename)) {
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
      for (const [ key, value ] of ObjectEntries(options.env))
        env[key] = `${value}`;
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
                                   parseResourceLimits(options.resourceLimits));
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
      transferList.push(...options.transferList);

    this[kPublicPort] = port1;
    this[kPublicPort].on('message', (message) => this.emit('message', message));
    setupPortReferencing(this[kPublicPort], this, 'message');
    this[kPort].postMessage({
      argv,
      type: messageTypes.LOAD_SCRIPT,
      filename,
      doEval: !!options.eval,
      cwdCounter: cwdCounter || workerIo.sharedCwdCounter,
      workerData: options.workerData,
      publicPort: port2,
      manifestSrc: getOptionValue('--experimental-policy') ?
        require('internal/process/policy').src :
        null,
      hasStdin: !!options.stdin
    }, transferList);
    // Actually start the new thread now that everything is in place.
    this[kHandle].startThread();
  }

  [kOnExit](code, customErr, customErrReason) {
    debug(`[${threadId}] hears end event for Worker ${this.threadId}`);
    drainMessagePort(this[kPublicPort]);
    drainMessagePort(this[kPort]);
    this[kDispose]();
    if (customErr) {
      debug(`[${threadId}] failing with custom error ${customErr} \
        and with reason {customErrReason}`);
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
        return this.emit('online');
      case messageTypes.COULD_NOT_SERIALIZE_ERROR:
        return this[kOnCouldNotSerializeErr]();
      case messageTypes.ERROR_MESSAGE:
        return this[kOnErrorMessage](message.error);
      case messageTypes.STDIO_PAYLOAD:
      {
        const { stream, chunk, encoding } = message;
        return this[kParentSideStdio][stream].push(chunk, encoding);
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

    this[kPublicPort].postMessage(...args);
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
  ret.fill(-1);
  if (typeof obj !== 'object' || obj === null) return ret;

  if (typeof obj.maxOldGenerationSizeMb === 'number')
    ret[kMaxOldGenerationSizeMb] = MathMax(obj.maxOldGenerationSizeMb, 2);
  if (typeof obj.maxYoungGenerationSizeMb === 'number')
    ret[kMaxYoungGenerationSizeMb] = obj.maxYoungGenerationSizeMb;
  if (typeof obj.codeRangeSizeMb === 'number')
    ret[kCodeRangeSizeMb] = obj.codeRangeSizeMb;
  return ret;
}

function makeResourceLimits(float64arr) {
  return {
    maxYoungGenerationSizeMb: float64arr[kMaxYoungGenerationSizeMb],
    maxOldGenerationSizeMb: float64arr[kMaxOldGenerationSizeMb],
    codeRangeSizeMb: float64arr[kCodeRangeSizeMb]
  };
}

module.exports = {
  ownsProcessState,
  isMainThread,
  SHARE_ENV,
  resourceLimits:
    !isMainThread ? makeResourceLimits(resourceLimitsRaw) : {},
  threadId,
  Worker,
};
