'use strict';

const {
  ERR_INSPECTOR_CLOSED,
  ERR_INSPECTOR_NOT_CONNECTED,
  ERR_INVALID_ARG_TYPE
} = require('internal/errors').codes;
const { validateString } = require('internal/validators');
const { url } = process.binding('inspector');
const { originalConsole } = require('internal/process/per_thread');
const {
  BaseSession,
  open,
  connectionSymbol,
  onMessageSymbol,
  nextIdSymbol
} = require('internal/inspector/utils');

const messagePromisesSymbol = Symbol('messagePromises');

class Session extends BaseSession {
  constructor() {
    super();
    this[messagePromisesSymbol] = new Map();
  }

  [onMessageSymbol](message) {
    const parsed = JSON.parse(message);
    try {
      if (parsed.id) {
        const fn = this[messagePromisesSymbol].get(parsed.id);
        this[messagePromisesSymbol].delete(parsed.id);
        if (parsed.error)
          fn.reject(parsed.error);
        else
          fn.resolve(parsed.result);
      } else {
        this.emit(parsed.method, parsed);
        this.emit('inspectorNotification', parsed);
      }
    } catch (error) {
      process.emitWarning(error);
    }
  }

  async post(method, params) {
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

    const fn = {
      resolve: null,
      reject: null,
    };
    const p = new Promise((resolve, reject) => {
      fn.resolve = resolve;
      fn.reject = reject;
    });
    this[messagePromisesSymbol].set(id, fn);

    this[connectionSymbol].dispatch(JSON.stringify(message));
    return p;
  }

  disconnect() {
    if (!this[connectionSymbol])
      return;
    this[connectionSymbol].disconnect();
    this[connectionSymbol] = null;
    const remainingPromises = this[messagePromisesSymbol].values();
    for (const fn of remainingPromises) {
      fn.reject(new ERR_INSPECTOR_CLOSED());
    }
    this[messagePromisesSymbol].clear();
    this[nextIdSymbol] = 1;
  }
}

module.exports = {
  open,
  close: process._debugEnd,
  url,
  console: originalConsole,
  Session
};
