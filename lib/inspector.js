'use strict';

const EventEmitter = require('events');
const util = require('util');
const { connect, open, url } = process.binding('inspector');

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

module.exports = {
  open: (port, host, wait) => open(port, host, !!wait),
  close: process._debugEnd,
  url: url,
  Session
};
