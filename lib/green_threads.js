'use strict';

const {
  ObjectKeys,
  PromisePrototypeThen,
  PromiseReject,
  PromiseResolve,
  PromiseWithResolvers,
  ReflectApply,
  SafeMap,
  SafeWeakMap,
  SymbolAsyncDispose,
} = primordials;

const EventEmitter = require('events');
const {
  codes: {
    ERR_GREEN_THREAD_CPU_LIMIT,
    ERR_GREEN_THREAD_EXITED,
    ERR_GREEN_THREAD_ISOLATE_GROUPS_UNSUPPORTED,
    ERR_GREEN_THREAD_OUT_OF_MEMORY,
    ERR_GREEN_THREAD_RPC_DISPOSED,
    ERR_GREEN_THREAD_RPC_NO_SUCH_METHOD,
    ERR_GREEN_THREAD_RPC_UNKNOWN_EXPORT,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_RETURN_PROPERTY_VALUE,
    ERR_INVALID_RETURN_VALUE,
  },
} = require('internal/errors');
const {
  emitExperimentalWarning,
  kEmptyObject,
  lazyDOMExceptionClass,
} = require('internal/util');
const {
  AbortController,
  AbortSignal,
} = require('internal/abort_controller');
const { createRpc } = require('internal/green_threads/rpc');
const { kTestingFlags } = require('internal/green_threads/testing');
const { isAbsolute } = require('path');
const { URL, pathToFileURL } = require('internal/url');
const { isAnyArrayBuffer, isArrayBufferView } = require('internal/util/types');
const {
  validateFunction,
  validateInteger,
  validateNumber,
  validateOneOf,
  validateObject,
  validateString,
} = require('internal/validators');
const {
  GreenThread: GreenThreadHandle,
  kEventMessage,
  kEventConsole,
  kEventEvalResult,
  kEventError,
  kEventExit,
  kEventAbandoned,
  kEventRpc,
  kErrorOutOfMemory,
  kErrorCpuLimit,
  setWarmPoolSize: setWarmPoolSizeBinding,
  setUnresponsiveTimeouts: setUnresponsiveTimeoutsBinding,
  WarmPoolWaiter,
  IsolateGroup: IsolateGroupHandle,
  isolateGroupsSupported,
  kIsolateGroupDefault,
  kIsolateGroupNew,
  kIsolateGroupShared,
  rpcBrand,
} = internalBinding('green_thread');

emitExperimentalWarning('Green threads');

const kConsoleMethods = ['log', 'info', 'warn', 'error', 'debug'];

let hostConsole;

const isolateGroupHandles = new SafeWeakMap();

const { RpcTarget, RpcSession } = createRpc({
  brand: rpcBrand,
  AbortController,
  AbortSignal,
  DOMException: lazyDOMExceptionClass(),
  errors: {
    disposed: () => new ERR_GREEN_THREAD_RPC_DISPOSED(),
    noSuchMethod: (name) => new ERR_GREEN_THREAD_RPC_NO_SUCH_METHOD(name),
    unknownExport: () => new ERR_GREEN_THREAD_RPC_UNKNOWN_EXPORT(),
  },
});

/**
 * A V8 isolate group that green threads can share. Isolates in the same
 * group share a pointer compression cage, which limits their heaps to 4 GB
 * together, and the read-only part of their heaps.
 */
class IsolateGroup {
  constructor() {
    if (!isolateGroupsSupported) {
      throw new ERR_GREEN_THREAD_ISOLATE_GROUPS_UNSUPPORTED();
    }
    isolateGroupHandles.set(this, new IsolateGroupHandle());
  }

  /**
   * Whether this build of Node.js can create isolate groups.
   * @type {boolean}
   */
  static get supported() {
    return isolateGroupsSupported;
  }
}

function getIsolateGroupArgs(isolateGroup) {
  if (isolateGroup === undefined) return [undefined, undefined];
  if (isolateGroup === 'default') return [kIsolateGroupDefault, undefined];
  if (isolateGroup === 'new') {
    if (!isolateGroupsSupported) {
      throw new ERR_GREEN_THREAD_ISOLATE_GROUPS_UNSUPPORTED();
    }
    return [kIsolateGroupNew, undefined];
  }
  const handle = isolateGroupHandles.get(isolateGroup);
  if (handle === undefined) {
    throw new ERR_INVALID_ARG_VALUE('options.isolateGroup', isolateGroup,
                                    "must be 'default', 'new', or an " +
                                    'IsolateGroup');
  }
  return [kIsolateGroupShared, handle];
}

// The href of a module's URL, which may be given as an absolute path.
function toModuleUrl(value, name) {
  validateString(value, name);
  if (URL.canParse(value)) return new URL(value).href;
  if (isAbsolute(value)) return pathToFileURL(value).href;
  throw new ERR_INVALID_ARG_VALUE(name, value,
                                  'must be an absolute URL or path');
}

let utf8Decoder;

// A module in the bundle, or what the load() hook returned, as passed to the
// green thread: its source as a string, or { source, format }.
function toModuleEntry(entry, invalid) {
  if (typeof entry === 'string') return entry;
  if (typeof entry !== 'object' || entry === null) {
    throw invalid('a string or an object', undefined, entry);
  }
  let { source } = entry;
  const { format = 'module' } = entry;
  if (isAnyArrayBuffer(source) || isArrayBufferView(source)) {
    utf8Decoder ??= new (require('internal/encoding').TextDecoder)();
    source = utf8Decoder.decode(source);
  } else if (typeof source !== 'string') {
    throw invalid('a string, ArrayBuffer, or Uint8Array', 'source', source);
  }
  if (format !== 'module' && format !== 'json') {
    throw invalid("'module' or 'json'", 'format', format);
  }
  return format === 'module' ? source : { source, format };
}

function toBundle(modules) {
  validateObject(modules, 'options.modules');
  const bundle = { __proto__: null };
  const keys = ObjectKeys(modules);
  for (let i = 0; i < keys.length; i++) {
    const name = `options.modules[${keys[i]}]`;
    bundle[toModuleUrl(keys[i], name)] = toModuleEntry(
      modules[keys[i]], (expected, property, value) => {
        return new ERR_INVALID_ARG_TYPE(
          property === undefined ? name : `${name}.${property}`,
          expected, value);
      });
  }
  return bundle;
}

// The host's module loader hooks, which a green thread calls through RPC.
class ModuleLoader extends RpcTarget {
  #resolve;
  #load;

  constructor(resolve, load) {
    super();
    this.#resolve = resolve;
    this.#load = load;
  }

  async resolve(specifier, context) {
    const result = await this.#resolve(specifier, context);
    if (result === undefined) return undefined;
    if (typeof result !== 'object' || result === null) {
      throw new ERR_INVALID_RETURN_VALUE('an object or undefined', 'resolve',
                                         result);
    }
    if (typeof result.url !== 'string' || !URL.canParse(result.url)) {
      throw new ERR_INVALID_RETURN_PROPERTY_VALUE('an absolute URL string',
                                                  'resolve', 'url',
                                                  result.url);
    }
    return { __proto__: null, url: new URL(result.url).href };
  }

  async load(url, context) {
    const result = await this.#load(url, context);
    if (result === undefined) return undefined;
    if (typeof result !== 'object' || result === null) {
      throw new ERR_INVALID_RETURN_VALUE('an object or undefined', 'load',
                                         result);
    }
    return toModuleEntry(result, (expected, property, value) => {
      return new ERR_INVALID_RETURN_PROPERTY_VALUE(expected, 'load',
                                                   property, value);
    });
  }
}

class GreenThread extends EventEmitter {
  #handle;
  #rpc;
  #exitCode;
  #exports;
  #exited = PromiseWithResolvers();
  #nextEvalId = 0;
  #pendingEvals = new SafeMap();

  /**
   * @param {string} source
   * @param {{
   *   env?: object,
   *   filename?: string,
   *   isolateGroup?: 'default' | 'new' | IsolateGroup,
   *   resourceLimits?: {
   *     maxOldGenerationSizeMb?: number,
   *     maxYoungGenerationSizeMb?: number,
   *     maxMemoryMb?: number,
   *     maxCpuTimeMs?: number,
   *     maxTaskCpuTimeMs?: number,
   *   },
   * }} [options]
   */
  constructor(source, options = kEmptyObject) {
    super();
    validateString(source, 'source');
    validateObject(options, 'options');
    const {
      env,
      type = 'script',
      isolateGroup,
      resourceLimits = kEmptyObject,
      modules,
      resolve,
      load,
    } = options;
    let { filename } = options;
    const { 0: groupKind, 1: groupHandle } = getIsolateGroupArgs(isolateGroup);
    validateOneOf(type, 'options.type', ['script', 'module']);
    if (type === 'module') {
      filename = toModuleUrl(filename ?? 'file:///green-thread.js',
                             'options.filename');
    } else {
      filename ??= 'green-thread.js';
      validateString(filename, 'options.filename');
    }
    if (env !== undefined) validateObject(env, 'options.env');
    if (resolve !== undefined) validateFunction(resolve, 'options.resolve');
    if (load !== undefined) validateFunction(load, 'options.load');
    const init = { __proto__: null };
    let hasInit = false;
    if (env !== undefined) {
      init.env = env;
      hasInit = true;
    }
    if (modules !== undefined) {
      init.modules = toBundle(modules);
      hasInit = true;
    }
    if (resolve !== undefined || load !== undefined) {
      init.loader = new ModuleLoader(resolve, load);
      init.hasResolve = resolve !== undefined;
      init.hasLoad = load !== undefined;
      hasInit = true;
    }
    validateObject(resourceLimits, 'options.resourceLimits');
    const {
      maxOldGenerationSizeMb = 0,
      maxYoungGenerationSizeMb = 0,
      maxMemoryMb = 0,
      maxCpuTimeMs = 0,
      maxTaskCpuTimeMs = 0,
    } = resourceLimits;
    validateNumber(maxOldGenerationSizeMb,
                   'options.resourceLimits.maxOldGenerationSizeMb', 0);
    validateNumber(maxYoungGenerationSizeMb,
                   'options.resourceLimits.maxYoungGenerationSizeMb', 0);
    validateNumber(maxMemoryMb, 'options.resourceLimits.maxMemoryMb', 0);
    validateNumber(maxCpuTimeMs, 'options.resourceLimits.maxCpuTimeMs', 0);
    validateNumber(maxTaskCpuTimeMs,
                   'options.resourceLimits.maxTaskCpuTimeMs', 0);

    this.#rpc = new RpcSession((message, isCall, context = 0) => {
      this.#handle.rpc(message, context);
    });
    if (type === 'module') {
      this.#exports = PromiseWithResolvers();
      // Rejections that nothing waits for are reported as 'error' events.
      PromisePrototypeThen(this.#exports.promise, undefined, () => {});
      this.#rpc.onexports = (value) => this.#exports.resolve(value);
    }
    this.#handle = new GreenThreadHandle(source,
                                         filename,
                                         maxOldGenerationSizeMb,
                                         maxYoungGenerationSizeMb,
                                         options[kTestingFlags] ?? 0,
                                         maxMemoryMb,
                                         maxCpuTimeMs,
                                         maxTaskCpuTimeMs,
                                         groupKind,
                                         groupHandle,
                                         this.#rpc,
                                         hasInit ? this.#rpc.wrapEnv(init) :
                                           undefined,
                                         type === 'module');
    this.#handle.onevent = (type, id, ok, value) => {
      this.#onEvent(type, id, ok, value);
    };
  }

  /**
   * For module green threads, a promise for the main module's exports, once
   * it has been evaluated. Functions are stubs, whose calls run in the green
   * thread, and other values are copies. `undefined` for scripts.
   * @type {Promise<object>|undefined}
   */
  get exports() {
    return this.#exports?.promise;
  }

  /**
   * The exit code, or `undefined` while the green thread is running.
   * @type {number|undefined}
   */
  get exitCode() {
    return this.#exitCode;
  }

  /**
   * Sends a structured clone of `value` to the green thread's `onmessage`.
   * @param {any} value
   * @param {ArrayBuffer[] | { transfer?: ArrayBuffer[] }} [transfer]
   *   ArrayBuffers to transfer rather than copy.
   */
  postMessage(value, transfer) {
    if (this.#exitCode !== undefined) return;
    this.#handle.postMessage(value, transfer);
  }

  /**
   * Runs `source` as a script in the green thread. Resolves with a
   * structured clone of the completion value, awaiting it if it is a promise.
   * @param {string} source
   * @param {{ filename?: string }} [options]
   * @returns {Promise<any>}
   */
  evaluate(source, options = kEmptyObject) {
    validateString(source, 'source');
    validateObject(options, 'options');
    const { filename = 'green-thread-eval.js' } = options;
    validateString(filename, 'options.filename');
    if (this.#exitCode !== undefined) {
      return PromiseReject(new ERR_GREEN_THREAD_EXITED());
    }
    const id = ++this.#nextEvalId;
    const pending = PromiseWithResolvers();
    this.#pendingEvals.set(id, pending);
    this.#handle.evaluate(id, source, filename);
    return pending.promise;
  }

  /**
   * Stops the green thread, interrupting any running code.
   * @returns {Promise<number>} The exit code.
   */
  terminate() {
    if (this.#exitCode !== undefined) return PromiseResolve(this.#exitCode);
    this.#handle.terminate();
    return this.#exited.promise;
  }

  ref() {
    if (this.#exitCode === undefined) this.#handle.ref();
    return this;
  }

  unref() {
    if (this.#exitCode === undefined) this.#handle.unref();
    return this;
  }

  async [SymbolAsyncDispose]() {
    await this.terminate();
  }

  #onEvent(type, id, ok, value) {
    switch (type) {
      case kEventMessage:
        this.emit('message', value);
        break;
      case kEventConsole: {
        hostConsole ??= require('internal/console/global');
        ReflectApply(hostConsole[kConsoleMethods[id]], hostConsole, value);
        break;
      }
      case kEventEvalResult: {
        const pending = this.#pendingEvals.get(id);
        if (pending === undefined) break;
        this.#pendingEvals.delete(id);
        if (ok) {
          pending.resolve(value);
        } else {
          pending.reject(value);
        }
        break;
      }
      case kEventError:
        if (id === kErrorOutOfMemory) {
          value = new ERR_GREEN_THREAD_OUT_OF_MEMORY();
        } else if (id === kErrorCpuLimit) {
          value = new ERR_GREEN_THREAD_CPU_LIMIT();
        }
        this.#exports?.reject(value);
        this.emit('error', value);
        break;
      case kEventAbandoned:
        process.emitWarning(
          `A green thread did not stop within ${id} ms of being terminated ` +
          'and was abandoned. It keeps the memory it uses until the process ' +
          'exits.', 'GreenThreadWarning');
        break;
      case kEventRpc:
        this.#rpc.onMessage(value, id);
        break;
      case kEventExit:
        this.#onExit(id);
        break;
    }
  }

  #onExit(code) {
    this.#exitCode = code;
    this.#handle.close();
    this.#rpc.close(new ERR_GREEN_THREAD_EXITED());
    for (const { reject } of this.#pendingEvals.values()) {
      reject(new ERR_GREEN_THREAD_EXITED());
    }
    this.#pendingEvals.clear();
    this.#exports?.reject(new ERR_GREEN_THREAD_EXITED());
    this.#exited.resolve(code);
    this.emit('exit', code);
  }
}

/**
 * Sets how many green thread isolates are kept initialized ahead of time,
 * for the whole process.
 * @param {number} size
 * @returns {Promise<void>} Fulfilled once the pool holds `size` initialized
 *   isolates.
 */
function setWarmPoolSize(size) {
  validateInteger(size, 'size', 0, 1024);
  const { promise, resolve } = PromiseWithResolvers();
  const waiter = new WarmPoolWaiter();
  waiter.onready = () => {
    waiter.close();
    resolve();
  };
  setWarmPoolSizeBinding(size, waiter);
  return promise;
}

/**
 * Sets, for the whole process, how long green threads that don't stop when
 * asked to have before they are abandoned, and how long Node.js then tries to
 * stop the pool thread running them before it aborts the process.
 * @param {{
 *   gracePeriodMs?: number,
 *   abortTimeoutMs?: number,
 * }} timeouts
 */
function setUnresponsiveTimeouts(timeouts) {
  validateObject(timeouts, 'timeouts');
  const { gracePeriodMs, abortTimeoutMs } = timeouts;
  if (gracePeriodMs !== undefined) {
    validateInteger(gracePeriodMs, 'timeouts.gracePeriodMs', 1, 2 ** 31 - 1);
  }
  if (abortTimeoutMs !== undefined && abortTimeoutMs !== Infinity) {
    validateInteger(abortTimeoutMs, 'timeouts.abortTimeoutMs', 0,
                    2 ** 31 - 1);
  }
  setUnresponsiveTimeoutsBinding(gracePeriodMs, abortTimeoutMs);
}

module.exports = {
  GreenThread,
  IsolateGroup,
  RpcTarget,
  setUnresponsiveTimeouts,
  setWarmPoolSize,
};
