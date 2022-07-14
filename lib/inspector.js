'use strict';

const {
  JSONParse,
  JSONStringify,
  SafeMap,
  Symbol,
} = primordials;

const {
  ERR_INSPECTOR_ALREADY_ACTIVATED,
  ERR_INSPECTOR_ALREADY_CONNECTED,
  ERR_INSPECTOR_CLOSED,
  ERR_INSPECTOR_COMMAND,
  ERR_INSPECTOR_NOT_AVAILABLE,
  ERR_INSPECTOR_NOT_CONNECTED,
  ERR_INSPECTOR_NOT_ACTIVE,
  ERR_INSPECTOR_NOT_WORKER,
} = require('internal/errors').codes;

const { hasInspector } = internalBinding('config');
if (!hasInspector)
  throw new ERR_INSPECTOR_NOT_AVAILABLE();

const EventEmitter = require('events');
const { queueMicrotask } = require('internal/process/task_queues');
const {
  validateFunction,
  validateObject,
  validateString,
} = require('internal/validators');
const { isMainThread } = require('worker_threads');

const {
  Connection,
  MainThreadConnection,
  open,
  url,
  isEnabled,
  waitForDebugger,
  console,
} = internalBinding('inspector');

const connectionSymbol = Symbol('connectionProperty');
const messageCallbacksSymbol = Symbol('messageCallbacks');
const nextIdSymbol = Symbol('nextId');
const onMessageSymbol = Symbol('onMessage');

class Session extends EventEmitter {
  constructor() {
    super();
    this[connectionSymbol] = null;
    this[nextIdSymbol] = 1;
    this[messageCallbacksSymbol] = new SafeMap();
  }

  /**
   * Connects the session to the inspector back-end.
   * @returns {void}
   */
  connect() {
    if (this[connectionSymbol])
      throw new ERR_INSPECTOR_ALREADY_CONNECTED('The inspector session');
    this[connectionSymbol] =
      new Connection((message) => this[onMessageSymbol](message));
  }

  /**
   * Connects the session to the main thread
   * inspector back-end.
   * @returns {void}
   */
  connectToMainThread() {
    if (isMainThread)
      throw new ERR_INSPECTOR_NOT_WORKER();
    if (this[connectionSymbol])
      throw new ERR_INSPECTOR_ALREADY_CONNECTED('The inspector session');
    this[connectionSymbol] =
      new MainThreadConnection(
        (message) => queueMicrotask(() => this[onMessageSymbol](message)));
  }

  [onMessageSymbol](message) {
    const parsed = JSONParse(message);
    try {
      if (parsed.id) {
        const callback = this[messageCallbacksSymbol].get(parsed.id);
        this[messageCallbacksSymbol].delete(parsed.id);
        if (callback) {
          if (parsed.error) {
            return callback(new ERR_INSPECTOR_COMMAND(parsed.error.code,
                                                      parsed.error.message));
          }

          callback(null, parsed.result);
        }
      } else {
        this.emit(parsed.method, parsed);
        this.emit('inspectorNotification', parsed);
      }
    } catch (error) {
      process.emitWarning(error);
    }
  }

  /**
   * Posts a message to the inspector back-end.
   * @param {string} method
   * @param {Record<unknown, unknown>} [params]
   * @param {Function} [callback]
   * @returns {void}
   */
  post(method, params, callback) {
    validateString(method, 'method');
    if (!callback && typeof params === 'function') {
      callback = params;
      params = null;
    }
    if (params) {
      validateObject(params, 'params');
    }
    if (callback) {
      validateFunction(callback, 'callback');
    }

    if (!this[connectionSymbol]) {
      throw new ERR_INSPECTOR_NOT_CONNECTED();
    }
    const id = this[nextIdSymbol]++;
    const message = { id, method };
    if (params) {
      message.params = params;
    }
    if (callback) {
      this[messageCallbacksSymbol].set(id, callback);
    }
    this[connectionSymbol].dispatch(JSONStringify(message));
  }

  /**
   * Immediately closes the session, all pending
   * message callbacks will be called with an
   * error.
   * @returns {void}
   */
  disconnect() {
    if (!this[connectionSymbol])
      return;
    this[connectionSymbol].disconnect();
    this[connectionSymbol] = null;
    const remainingCallbacks = this[messageCallbacksSymbol].values();
    for (const callback of remainingCallbacks) {
      process.nextTick(callback, new ERR_INSPECTOR_CLOSED());
    }
    this[messageCallbacksSymbol].clear();
    this[nextIdSymbol] = 1;
  }
}

/**
 * Activates inspector on host and port.
 * @param {number} [port]
 * @param {string} [host]
 * @param {boolean} [wait]
 * @returns {void}
 */
function inspectorOpen(port, host, wait) {
  if (isEnabled()) {
    throw new ERR_INSPECTOR_ALREADY_ACTIVATED();
  }
  open(port, host);
  if (wait)
    waitForDebugger();
}

/**
 * Blocks until a client (existing or connected later)
 * has sent the `Runtime.runIfWaitingForDebugger`
 * command.
 * @returns {void}
 */
function inspectorWaitForDebugger() {
  if (!waitForDebugger())
    throw new ERR_INSPECTOR_NOT_ACTIVE();
}

module.exports = {
  open: inspectorOpen,
  close: process._debugEnd,
  url,
  waitForDebugger: inspectorWaitForDebugger,
  console,
  Session
};
