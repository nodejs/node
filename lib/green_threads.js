'use strict';

const {
  PromiseReject,
  PromiseResolve,
  PromiseWithResolvers,
  ReflectApply,
  SafeMap,
  SymbolAsyncDispose,
} = primordials;

const EventEmitter = require('events');
const {
  codes: {
    ERR_GREEN_THREAD_CPU_LIMIT,
    ERR_GREEN_THREAD_EXITED,
    ERR_GREEN_THREAD_OUT_OF_MEMORY,
  },
} = require('internal/errors');
const {
  emitExperimentalWarning,
  kEmptyObject,
} = require('internal/util');
const {
  validateInteger,
  validateNumber,
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
  kErrorOutOfMemory,
  kErrorCpuLimit,
  setWarmPoolSize: setWarmPoolSizeBinding,
} = internalBinding('green_thread');

emitExperimentalWarning('Green threads');

const kConsoleMethods = ['log', 'info', 'warn', 'error', 'debug'];

let hostConsole;

class GreenThread extends EventEmitter {
  #handle;
  #exitCode;
  #exited = PromiseWithResolvers();
  #nextEvalId = 0;
  #pendingEvals = new SafeMap();

  /**
   * @param {string} source
   * @param {{
   *   filename?: string,
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
      filename = 'green-thread.js',
      resourceLimits = kEmptyObject,
    } = options;
    validateString(filename, 'options.filename');
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

    this.#handle = new GreenThreadHandle(source,
                                         filename,
                                         maxOldGenerationSizeMb,
                                         maxYoungGenerationSizeMb,
                                         0,
                                         maxMemoryMb,
                                         maxCpuTimeMs,
                                         maxTaskCpuTimeMs);
    this.#handle.onevent = (type, id, ok, value) => {
      this.#onEvent(type, id, ok, value);
    };
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
   */
  postMessage(value) {
    if (this.#exitCode !== undefined) return;
    this.#handle.postMessage(value);
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
        this.emit('error', value);
        break;
      case kEventExit:
        this.#onExit(id);
        break;
    }
  }

  #onExit(code) {
    this.#exitCode = code;
    this.#handle.close();
    for (const { reject } of this.#pendingEvals.values()) {
      reject(new ERR_GREEN_THREAD_EXITED());
    }
    this.#pendingEvals.clear();
    this.#exited.resolve(code);
    this.emit('exit', code);
  }
}

/**
 * Sets how many green thread isolates are kept initialized ahead of time,
 * for the whole process.
 * @param {number} size
 */
function setWarmPoolSize(size) {
  validateInteger(size, 'size', 0, 1024);
  setWarmPoolSizeBinding(size);
}

module.exports = {
  GreenThread,
  setWarmPoolSize,
};
