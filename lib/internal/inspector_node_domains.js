'use strict';

const assert = require('assert');
const util = require('util');

const { registerDispatcherFactory } = process.binding('inspector');

const domainHandlerClasses = new Map();
const SessionTerminatedSymbol = Symbol('DisposeAgent');

function createNotificationChannel(session, domain) {
  return function(method, params) {
    session.sendMessageToFrontend(JSON.stringify({
      method: `${domain}.${method}`,
      params
    }));
  };
}

function createResponseCallback(session, id) {
  let first_call = true;
  return function(error, result) {
    assert.ok(first_call, 'Callback should only be called once');
    first_call = false;
    const response = { id };
    if (error)
      response['error'] = error;
    else
      response['result'] = result || {};
    session.sendMessageToFrontend(JSON.stringify(response));
  };
}

function parseSuppressingError(message) {
  try {
    return JSON.parse(message);
  } catch (error) {
    process.emitWarning(error);
    return {};
  }
}

class Dispatcher {
  constructor(session) {
    this._session = session;
    this._domainHandlers = new Map();
  }

  dispatch(message) {
    if (domainHandlerClasses.size === 0 && this._domainHandlers.size === 0) {
      return false;
    }

    if (message === null) {
      this._shutdown();
      return;
    }
    const parsed = parseSuppressingError(message);
    const [ domain, method ] = parsed.method ? parsed.method.split('.') : [];

    if (!domain || !method)
      return false;

    const handler = this._getOrCreateHandler(domain);

    if (!handler || !util.isFunction(handler[method])) {
      return false;
    }

    const callback = createResponseCallback(this._session, parsed.id);
    try {
      const result = handler[method](parsed.params);
      if (util.isFunction(result)) {
        result(callback);
      } else {
        callback(null, result);
      }
    } catch (e) {
      callback(e.message || e, null);
    }
    return true;
  }

  _getOrCreateHandler(domain) {
    let handler = this._domainHandlers.get(domain);
    if (handler)
      return handler;
    var ctor = domainHandlerClasses.get(domain);
    if (!ctor)
      return null;
    handler = new ctor(createNotificationChannel(this._session, domain));
    this._domainHandlers.set(domain, handler);
    return handler;
  }

  _shutdown() {
    for (const handler of this._domainHandlers.values()) {
      if (handler[SessionTerminatedSymbol])
        handler[SessionTerminatedSymbol]();
    }
    this._domainHandlers.clear();
  }

  static create(session) {
    const dispatcher = new Dispatcher(session);
    return dispatcher.dispatch.bind(dispatcher);
  }
}

function registerDomainHandlerClass(domain, factory) {
  domainHandlerClasses.set(domain, factory);
}

function setup() {
  registerDispatcherFactory(Dispatcher.create);
}

module.exports = {
  registerDomainHandlerClass, setup, SessionTerminatedSymbol
};
