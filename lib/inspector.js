'use strict';

const errors = require('internal/errors');
const EventEmitter = require('events');
const util = require('util');
const { isContext } = process.binding('contextify');
const {
  connect,
  open,
  url,
  contextAttached: _contextAttached,
  attachContext: _attachContext,
  detachContext: _detachContext
} = process.binding('inspector');

if (!connect)
  throw new Error('Inspector is not available');

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
      throw new Error('Already connected');
    this[connectionSymbol] =
        connect((message) => this[onMessageSymbol](message));
  }

  [onMessageSymbol](message) {
    const parsed = JSON.parse(message);
    if (parsed.id) {
      const callback = this[messageCallbacksSymbol].get(parsed.id);
      this[messageCallbacksSymbol].delete(parsed.id);
      if (callback)
        callback(parsed.error || null, parsed.result || null);
    } else {
      this.emit(parsed.method, parsed);
      this.emit('inspectorNotification', parsed);
    }
  }

  post(method, params, callback) {
    if (typeof method !== 'string') {
      throw new TypeError(
        `"method" must be a string, got ${typeof method} instead`);
    }
    if (!callback && util.isFunction(params)) {
      callback = params;
      params = null;
    }
    if (params && typeof params !== 'object') {
      throw new TypeError(
        `"params" must be an object, got ${typeof params} instead`);
    }
    if (callback && typeof callback !== 'function') {
      throw new TypeError(
        `"callback" must be a function, got ${typeof callback} instead`);
    }

    if (!this[connectionSymbol]) {
      throw new Error('Session is not connected');
    }
    const id = this[nextIdSymbol]++;
    const message = { id, method };
    if (params) {
      message['params'] = params;
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
      process.nextTick(callback, new Error('Session was closed'));
    }
    this[messageCallbacksSymbol].clear();
    this[nextIdSymbol] = 1;
  }
}

function checkSandbox(sandbox) {
  if (typeof sandbox !== 'object') {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'sandbox',
                               'object', sandbox);
  }
  if (!isContext(sandbox)) {
    throw new errors.TypeError('ERR_SANDBOX_NOT_CONTEXTIFIED');
  }
}

let ctxIdx = 1;
function attachContext(contextifiedSandbox, {
  name = `vm Module Context ${ctxIdx++}`,
  origin
} = {}) {
  checkSandbox(contextifiedSandbox);
  if (typeof name !== 'string') {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'options.name',
                               'string', name);
  }
  if (origin !== undefined && typeof origin !== 'string') {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'options.origin',
                               'string', origin);
  }
  _attachContext(contextifiedSandbox, name, origin);
}

function detachContext(contextifiedSandbox) {
  checkSandbox(contextifiedSandbox);
  _detachContext(contextifiedSandbox);
}

function contextAttached(contextifiedSandbox) {
  checkSandbox(contextifiedSandbox);
  return _contextAttached(contextifiedSandbox);
}

module.exports = {
  open: (port, host, wait) => open(port, host, !!wait),
  close: process._debugEnd,
  url: url,
  Session,
  attachContext,
  detachContext,
  contextAttached
};
