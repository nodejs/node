'use strict';

const assert = require('assert');
const errors = require('internal/errors');
const EventEmitter = require('events');
const util = require('util');

const { registerDispatcherFactory } = process.binding('inspector');

const domainHandlerClasses = new Map();
const SessionTerminatedSymbol = Symbol('SessionTerminatedSymbol');
// V8 does not provide a way to obtain this list programmatically
const V8_DOMAINS = [ 'Runtime', 'Debugger', 'Profiler', 'HeapProfiler',
                     'Console', 'Schema' ];

function createNotificationChannel(session, domain) {
  return function(method, params) {
    session.sendMessageToFrontend(JSON.stringify({
      method: `${domain}.${method}`,
      params
    }));
  };
}

function createResponseCallback(session, id) {
  let firstCall = true;
  return function(error, result) {
    assert.ok(firstCall, 'Callback should only be called once');
    firstCall = false;
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

let targetIdCounter = 1;
let sessionIdCounter = 1;

const targetsAgent = new class extends EventEmitter {
  constructor() {
    super();
    this._targets = new Map();
  }

  add(info, attachCallback) {
    info.targetId = `target_${targetIdCounter++}`;
    this._targets.set(info.targetId, { info, attachCallback });
    this.emit('targetCreated', info);
    return {
      id: info.targetId,  // This is for test only
      destroyed: () => this._targetDestroyed(info),
      updated: this._targetUpdated.bind(this, info)
    };
  }

  attach(targetId, attachedCallback, messageCallback) {
    const target = this._targets.get(targetId);
    if (!target)
      throw new Error(`Target ${targetId} was not found`);
    target.attachCallback(attachedCallback, messageCallback);
  }

  targets() {
    return Array.from(this._targets.values()).map(({ info }) => info);
  }

  getTarget(id) {
    const target = this._targets.get(id);
    return target && target.info;
  }

  _targetDestroyed(target) {
    if (this._targets.delete(target.targetId)) {
      this.emit('targetDestroyed', target);
    }
  }

  _targetUpdated(info, type, title, url) {
    let updated = false;
    if (type && info.type !== type) {
      updated = true;
      info.type = type;
    }
    if (title && info.title !== title) {
      updated = true;
      info.title = title;
    }
    if (url && info.url !== url) {
      updated = true;
      info.url = url;
    }
    if (updated)
      this.emit('targetInfoChanged', info);
  }
}();

class TargetDomainHandler {
  constructor(notificationCallback) {
    this._notificationCallback = notificationCallback;
    this._discoverTargets = false;
    this._autoAttach = false;
    this._waitForDebuggerOnStart = false;
    this._listeners = [
      this._addListener('targetCreated',
                        (target) => this._onTargetCreated(target)),
      this._addListener('targetDestroyed',
                        (target) => this._onTargetDestroyed(target))
    ];
    this._discoveryListeners = [];

    this._sessions = new Map();
  }

  setDiscoverTargets({ discover }) {
    discover = !!discover;
    if (discover === this._discoverTargets)
      return;
    this._discoverTargets = discover;
    if (this._discoverTargets) {
      this._discoveryListeners = [
        this._forwardEventAsNotification('targetInfoChanged', true)
      ];
    } else {
      for (const remover of this._discoveryListeners) {
        remover();
      }
      this._discoveryListeners = [];
    }
  }

  setAutoAttach({ autoAttach, waitForDebuggerOnStart }) {
    this._autoAttach = !!autoAttach;
    this._waitForDebuggerOnStart = !!waitForDebuggerOnStart;
    if (this._autoAttach)
      return this._attachToAllTargets.bind(this);
    else
      return this._detachFromAllTargets.bind(this);
  }

  getTargets() {
    return targetsAgent.targets()
        .map((target) => this._toProtocolTarget(target));
  }

  getTargetInfo({ targetId }) {
    this._checkArgument('targetId', targetId);
    const target = targetsAgent.getTarget(targetId);
    if (target)
      return this._toProtocolTarget(target);
    else
      throw new Error(`Target ${targetId} was not found`);
  }

  attachToTarget({ targetId }) {
    this._checkArgument('targetId', targetId);
    return this._attachToTarget.bind(this, targetId);
  }

  detachFromTarget({ sessionId }) {
    return this._detachSession.bind(this, sessionId);
  }

  sendMessageToTarget({ sessionId, message }) {
    this._checkArgument('message', message);
    return (callback) => {
      this._getSessionChecked(sessionId).messageCallback(message, callback);
    };
  }

  setAttachToFrames() {
    // Not implemented, browser specific functionality
  }

  _checkArgument(name, value) {
    if (!value)
      throw new Error(`Argument ${name} was not provided`);
  }

  _attachToAllTargets(callback) {
    let count = 1;
    function decrease() {
      if ((--count) === 0) {
        callback();
      }
    }
    for (const target of targetsAgent.targets()) {
      count++;
      // Note that we don't care if attach fails
      try {
        this._attachToTarget(target.targetId, decrease);
      } catch (e) {
        decrease();
      }
    }
    decrease();
  }

  _detachFromAllTargets(callback) {
    let count = 1;
    function decrease() {
      if ((--count) === 0) {
        callback();
      }
    }
    for (const sessionId of this._sessions.keys()) {
      count++;
      try {
        this._detachSession(sessionId, decrease);
      } catch (e) {
        decrease();
      }
    }
    decrease();
  }

  _attachToTarget(targetId, callback) {
    const sessionId = `${targetId}:${sessionIdCounter++}`;
    const attachedCb = this._onSessionAttached.bind(this, targetId, sessionId,
                                                    callback);
    const messageCb = this._onMessage.bind(this, sessionId);
    targetsAgent.attach(targetId, attachedCb, messageCb);
  }

  _detachSession(sessionId, callback) {
    const { targetId, detachCallback } = this._getSessionChecked(sessionId);
    const target = this._toProtocolTarget(targetsAgent.getTarget(targetId));
    detachCallback((error) => {
      this._sessionDetached(sessionId, target);
      callback(error);
    });
  }

  _getSessionChecked(sessionId) {
    this._checkArgument('sessionId', sessionId);
    const session = this._sessions.get(sessionId);
    if (!session)
      throw new Error(`Session ${sessionId} was not found`);
    return session;
  }

  _onMessage(sessionId, message) {
    const session = this._sessions.get(sessionId);
    if (!session)
      return;
    if (message === null) {
      this._sessionDetached(sessionId,
                            targetsAgent.getTarget(session.targetId));
    } else {
      this._notificationCallback(
        'receivedMessageFromTarget', { sessionId, message });
    }

  }

  _onTargetCreated(target) {
    if (this._discoverTargets) {
      const targetInfo = this._toProtocolTarget(target);
      this._notificationCallback('targetCreated', { targetInfo });
    }
    if (this._autoAttach) {
      this._attachToTarget(target.targetId, () => {});
    }
  }

  _onTargetDestroyed(target) {
    for (const sessionId of this._sessionsForTarget(target.targetId)) {
      this._sessionDetached(sessionId, target);
    }
    if (this._discoverTargets) {
      const targetInfo = this._toProtocolTarget(target);
      this._notificationCallback('targetDestroyed', { targetInfo });
    }
  }

  _onSessionAttached(targetId, sessionId, callback, error, waitingForDebugger,
                     messageCallback, detachCallback) {
    if (error) {
      callback(error);
      return;
    }
    const session = { targetId, detachCallback, messageCallback };
    this._sessions.set(sessionId, session);
    const targetInfo = this._toProtocolTarget(targetsAgent.getTarget(targetId));
    if (this._sessionsForTarget(targetId).length === 1)
      this._sendTargetInfoChanged(targetInfo);
    this._notificationCallback('attachedToTarget',
                               { targetInfo, sessionId, waitingForDebugger });
    callback(null, { sessionId });
  }

  _sendTargetInfoChanged(targetInfo) {
    this._notificationCallback('targetInfoChanged', { targetInfo });
  }

  _sessionDetached(sessionId, target) {
    if (!this._sessions.has(sessionId))
      return;
    const { targetId } = this._sessions.get(sessionId);
    this._sessions.delete(sessionId);
    this._notificationCallback('detachedFromTarget', { sessionId });
    if (this._sessionsForTarget(targetId).length === 0) {
      this._sendTargetInfoChanged(this._toProtocolTarget(target));
    }
  }

  _removeListeners() {
    for (const remover of this._listeners) {
      remover();
    }
  }

  _forwardEventAsNotification(event, wrap) {
    const listener = (data) => {
      const targetInfo = this._toProtocolTarget(data);
      const object = wrap ? { targetInfo } : targetInfo;
      this._notificationCallback(event, object);
    };
    return this._addListener(event, listener);
  }

  _addListener(event, listener) {
    targetsAgent.addListener(event, listener);
    return () => {
      targetsAgent.removeListener(event, listener);
    };
  }

  _toProtocolTarget(target) {
    const attached = this._sessionsForTarget(target.targetId).length > 0;
    return Object.assign({ attached }, target);
  }

  _sessionsForTarget(tid) {
    return Array.from(this._sessions.entries())
        .filter(([, { targetId }]) => targetId === tid)
        .map(([ agentSessionId ]) => agentSessionId);
  }

  [SessionTerminatedSymbol]() {
    for (const { detachCallback } of this._sessions.values())
      detachCallback(() => {});
    this._removeListeners();
    this._listeners.forEach((cancel) => cancel());
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

    if (!domain || !method || !method.match(/^[a-zA-Z]\w*$/))
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

function newTarget(type, title, url, attachCallback) {
  return targetsAgent.add({ type, title, url }, attachCallback);
}

function registerDomainHandlerClass(domain, factory) {
  if (V8_DOMAINS.includes('domain'))
    throw errors.Error('ERR_INSPECTOR_NO_DOMAIN_OVERRIDE');
  domainHandlerClasses.set(domain, factory);
}

function setup() {
  registerDomainHandlerClass('Target', TargetDomainHandler);
  registerDispatcherFactory(Dispatcher.create);
}

module.exports = {
  newTarget, registerDomainHandlerClass, setup, SessionTerminatedSymbol
};
