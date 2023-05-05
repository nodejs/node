'use strict';

// LocalWorker was originally a separate module developed by
// Anna Henningsen and published separately on npm as the
// synchronous-worker module under the MIT license. It has been
// incorporated into Node.js with Anna's permission.
// See the LICENSE file for LICENSE and copyright attribution.

const {
  Promise,
} = primordials;

const {
  LocalWorker: LocalWorkerImpl,
} = internalBinding('contextify');

const EventEmitter = require('events');
const { setTimeout } = require('timers');
const { dirname, join } = require('path');

let debug = require('internal/util/debuglog').debuglog('localworker', (fn) => {
  debug = fn;
});

class LocalWorker extends EventEmitter {
  #handle = undefined;
  #process = undefined;
  #global = undefined;
  #module = undefined;
  #stoppedPromise = undefined;

  /**
   */
  constructor() {
    super();
    this.#handle = new LocalWorkerImpl();
    this.#handle.onexit = (code) => {
      this.stop();
      this.emit('exit', code);
    };
    try {
      this.#handle.start();
      this.#handle.load((process, nativeRequire, globalThis) => {
        this.#process = process;
        this.#module = nativeRequire('module');
        this.#global = globalThis;
        process.on('uncaughtException', (err) => {
          if (process.listenerCount('uncaughtException') === 1) {
            // If we are stopping, silence all errors
            if (!this.#stoppedPromise) {
              this.emit('error', err);
            }
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
   * @returns {Promise<void>}
   */
  async stop() {
    // TODO(@mcollina): add support for AbortController, we want to abort this,
    // or add a timeout.
    return this.#stoppedPromise ??= new Promise((resolve) => {
      const tryClosing = () => {
        const closed = this.#handle.tryCloseAllHandles();
        debug('closed %d handles', closed);
        if (closed > 0) {
          // This is an active wait for the handles to close.
          // We might want to change this in the future to use a callback,
          // but at this point it seems like a premature optimization.
          // TODO(@mcollina): refactor to use a close callback
          setTimeout(tryClosing, 100);
        } else {

          this.#handle.stop();
          resolve();
        }
      };

      // We use setTimeout instead of setImmediate because it runs in a different
      // phase of the event loop. This is important because the immediate queue
      // would crash if the environment it refers to has been already closed.
      setTimeout(tryClosing, 100);
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
   * @param {string} filename
   */
  async createImport(filename) {
    // This is a hack to get around creating a dynamic import function
    // from code. We create a temporary file that exports the import
    // function, and then delete it.
    // TODO(@mcollina): figure out how to do this using internal APIs.

    const req = this.createRequire(filename);
    const fs = req('fs/promises');

    const sourceText = `
      module.exports = (file) => import(file);
    `;

    const dest = join(dirname(filename), `_import_jump_${process.pid}.js`);
    await fs.writeFile(dest, sourceText);

    const ownImport = req(dest);

    await fs.unlink(dest);

    return ownImport;
  }
}

module.exports = LocalWorker;
