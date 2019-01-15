'use strict';

const {
  ERR_INSPECTOR_ALREADY_CONNECTED,
  ERR_INSPECTOR_CLOSED,
  ERR_INSPECTOR_NOT_AVAILABLE,
  ERR_INSPECTOR_NOT_CONNECTED,
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_CALLBACK
} = require('internal/errors').codes;

const { hasInspector } = internalBinding('config');
if (!hasInspector)
  throw new ERR_INSPECTOR_NOT_AVAILABLE();

const EventEmitter = require('events');
const { validateString } = require('internal/validators');
const util = require('util');
const { Connection, open, url } = internalBinding('inspector');

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
    const connection =
      new Connection((message) => this[onMessageSymbol](message));
    if (connection.sessionAttached) {
      throw new ERR_INSPECTOR_ALREADY_CONNECTED('Another inspector session');
    }
    this[connectionSymbol] = connection;
  }

  [onMessageSymbol](message) {
    const parsed = JSON.parse(message);
    try {
      if (parsed.id) {
        const callback = this[messageCallbacksSymbol].get(parsed.id);
        this[messageCallbacksSymbol].delete(parsed.id);
        if (callback)
          callback(parsed.error || null, parsed.result || null);
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
    if (!callback && util.isFunction(params)) {
      callback = params;
      params = null;
    }
    if (params && typeof params !== 'object') {
      throw new ERR_INVALID_ARG_TYPE('params', 'Object', params);
    }
    if (callback && typeof callback !== 'function') {
      throw new ERR_INVALID_CALLBACK();
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
    this[connectionSymbol].dispatch(JSON.stringify(message));
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

module.exports = {
  open: (port, host, wait) => open(port, host, !!wait),
  close: process._debugEnd,
  url: url,
  // This is dynamically added during bootstrap,
  // where the console from the VM is still available
  console: require('internal/util/inspector').consoleFromVM,
  Session
};
