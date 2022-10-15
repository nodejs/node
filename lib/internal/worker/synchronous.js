'use strict';

// SynchronousWorker was originally a separate module developed by
// Anna Henningsen and published separately on npm as the
// synchronous-worker module under the MIT license. It has been
// incorporated into Node.js with Anna's permission.
// See the LICENSE file for LICENSE and copyright attribution.

const {
  Promise,
} = primordials;

const {
  SynchronousWorker: SynchronousWorkerImpl,
  UV_RUN_DEFAULT,
  UV_RUN_ONCE,
  UV_RUN_NOWAIT,
} = internalBinding('worker');

const { setImmediate } = require('timers');

const EventEmitter = require('events');

const {
  codes: {
    ERR_INVALID_STATE,
  },
} = require('internal/errors');

class SynchronousWorker extends EventEmitter {
  #handle = undefined;
  #process = undefined;
  #global = undefined;
  #module = undefined;
  #hasOwnEventLoop = false;
  #hasOwnMicrotaskQueue = false;
  #promiseInspector = undefined;
  #stoppedPromise = undefined;

  /**
   * @typedef {{
   *   sharedEventLoop?: boolean,
   *   sharedMicrotaskQueue?: boolean,
   * }} SynchronousWorkerOptions
   * @param {SynchronousWorkerOptions} [options]
   */
  constructor(options) {
    super();
    this.#hasOwnEventLoop = !(options?.sharedEventLoop);
    this.#hasOwnMicrotaskQueue = !(options?.sharedMicrotaskQueue);

    this.#handle = new SynchronousWorkerImpl();
    this.#handle.onexit = (code) => {
      this.stop();
      this.emit('exit', code);
    };
    try {
      this.#handle.start(this.#hasOwnEventLoop, this.#hasOwnMicrotaskQueue);
      this.#handle.load((process, nativeRequire, globalThis) => {
        const origExit = process.reallyExit;
        process.reallyExit = (...args) => {
          const ret = origExit.call(process, ...args);
          // Make a dummy call to make sure the termination exception is
          // propagated. For some reason, this isn't necessarily the case
          // otherwise.
          process.memoryUsage();
          return ret;
        };
        this.#process = process;
        this.#module = nativeRequire('module');
        this.#global = globalThis;
        process.on('uncaughtException', (err) => {
          if (process.listenerCount('uncaughtException') === 1) {
            this.emit('error', err);
            process.exit(1);
          }
        });
      });
    } catch (err) {
      this.#handle.stop();
      throw err;
    }
  }

  /**
   * @param {'default'|'once'|'nowait'} [mode] = 'default'
   */
  runLoop(mode = 'default') {
    if (!this.#hasOwnEventLoop) {
      throw new ERR_INVALID_STATE('Can only use .runLoop() when using a separate event loop');
    }
    let uvMode = UV_RUN_DEFAULT;
    if (mode === 'once') uvMode = UV_RUN_ONCE;
    if (mode === 'nowait') uvMode = UV_RUN_NOWAIT;
    this.#handle.runLoop(uvMode);
  }

  /**
   * @param {Promise} promise
   */
  runLoopUntilPromiseResolved(promise) {
    if (!this.#hasOwnEventLoop || !this.#hasOwnMicrotaskQueue) {
      throw new ERR_INVALID_STATE(
        'Can only use .runLoopUntilPromiseResolved() when using a separate event loop ' +
        'and microtask queue');
    }
    this.#promiseInspector ??= this.createRequire('/worker.js')('vm').runInThisContext(
      `(promise => {
        const obj = { state: 'pending', value: null };
        promise.then((v) => { obj.state = 'fulfilled'; obj.value = v; },
                    (v) => { obj.state = 'rejected';  obj.value = v; });
        return obj;
      })`);
    const inspected = this.#promiseInspector(promise);
    this.runInWorkerScope(() => {}); // Flush the microtask queue
    while (inspected.state === 'pending') {
      this.runLoop('once');
    }
    if (inspected.state === 'rejected') {
      throw inspected.value;
    }
    return inspected.value;
  }

  /**
   * @type {boolean}
   */
  get loopAlive() {
    if (!this.#hasOwnEventLoop) {
      throw new ERR_INVALID_STATE('Can only use .loopAlive when using a separate event loop');
    }
    return this.#handle.isLoopAlive();
  }

  /**
   * @returns {Promise<void>}
   */
  async stop() {
    return this.#stoppedPromise ??= new Promise((resolve) => {
      this.#handle.signalStop();
      setImmediate(() => {
        this.#handle.stop();
        resolve();
      });
    });
  }

  get process() {
    return this.#process;
  }

  get globalThis() {
    return this.#global;
  }

  createRequire(...args) {
    return this.#module.createRequire(...args);
  }

  /**
   * @param {() => any} method
   */
  runInWorkerScope(method) {
    return this.#handle.runInCallbackScope(method);
  }
}

module.exports = SynchronousWorker;
