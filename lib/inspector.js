'use strict';

const { JSON } = primordials;

const {
  ERR_INSPECTOR_ALREADY_CONNECTED,
  ERR_INSPECTOR_CLOSED,
  ERR_INSPECTOR_COMMAND,
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
    this[connectionSymbol] =
      new Connection((message) => this[onMessageSymbol](message));
  }

  [onMessageSymbol](message) {
    const parsed = JSON.parse(message);
    try {
      if (parsed.id) {
        const {pResolve, pReject} = this[messageCallbacksSymbol].get(parsed.id);
        this[messageCallbacksSymbol].delete(parsed.id);
        if (parsed.error) {
          pReject(new ERR_INSPECTOR_COMMAND(parsed.error.code,
            parsed.error.message));
        } else {
          pResolve(parsed.result)
        }
      } else {
        this.emit(parsed.method, parsed);
        this.emit('inspectorNotification', parsed);
      }
    } catch (error) {
      process.emitWarning(error);
    }
  }

  post(method, params) {
    validateString(method, 'method');
    if (params && typeof params !== 'object') {
      throw new ERR_INVALID_ARG_TYPE('params', 'Object', params);
    }

    if (!this[connectionSymbol]) {
      throw new ERR_INSPECTOR_NOT_CONNECTED();
    }
    const id = this[nextIdSymbol]++;
    const message = { id, method };
    if (params) {
      message.params = params;
    }
    let pResolve, pReject;
    const p = new Promise((resolve, reject) => {
      pResolve = resolve;
      pReject = reject;

    });
    this[messageCallbacksSymbol].set(id, {pResolve, pReject});
    this[connectionSymbol].dispatch(JSON.stringify(message));

    return p;
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
