'use strict';

const {
  JSONParse,
  JSONStringify,
  Map,
  Symbol,
} = primordials;

const {
  ERR_INSPECTOR_ALREADY_CONNECTED,
  ERR_INSPECTOR_CLOSED,
  ERR_INSPECTOR_COMMAND,
  ERR_INSPECTOR_NOT_AVAILABLE,
  ERR_INSPECTOR_NOT_CONNECTED,
  ERR_INSPECTOR_NOT_ACTIVE,
  ERR_INSPECTOR_NOT_WORKER,
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_CALLBACK
} = require('internal/errors').codes;

const { hasInspector } = internalBinding('config');
if (!hasInspector)
  throw new ERR_INSPECTOR_NOT_AVAILABLE();

const EventEmitter = require('events');
const { queueMicrotask } = require('internal/process/task_queues');
const { validateString } = require('internal/validators');
const { isMainThread } = require('worker_threads');

const {
  Connection,
  MainThreadConnection,
  open,
  url,
  waitForDebugger
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
    this[messageCallbacksSymbol] = new Map();
  }

  connect() {
    if (this[connectionSymbol])
      throw new ERR_INSPECTOR_ALREADY_CONNECTED('The inspector session');
    this[connectionSymbol] =
      new Connection((message) => this[onMessageSymbol](message));
  }

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

  post(method, params, callback) {
    validateString(method, 'method');
    if (!callback && typeof params === 'function') {
      callback = params;
      params = null;
    }
    if (params && typeof params !== 'object') {
      throw new ERR_INVALID_ARG_TYPE('params', 'Object', params);
    }
    if (callback && typeof callback !== 'function') {
      throw new ERR_INVALID_CALLBACK(callback);
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

function inspectorOpen(port, host, wait) {
  open(port, host);
  if (wait)
    waitForDebugger();
}

function inspectorWaitForDebugger() {
  if (!waitForDebugger())
    throw new ERR_INSPECTOR_NOT_ACTIVE();
}

module.exports = {
  open: inspectorOpen,
  close: process._debugEnd,
  url: url,
  waitForDebugger: inspectorWaitForDebugger,
  // This is dynamically added during bootstrap,
  // where the console from the VM is still available
  console: require('internal/util/inspector').consoleFromVM,
  Session
};
