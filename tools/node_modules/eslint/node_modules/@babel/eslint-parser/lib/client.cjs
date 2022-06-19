var _class, _worker, _worker_threads, _worker_threads_cache;

function _classStaticPrivateFieldSpecSet(receiver, classConstructor, descriptor, value) { _classCheckPrivateStaticAccess(receiver, classConstructor); _classCheckPrivateStaticFieldDescriptor(descriptor, "set"); _classApplyDescriptorSet(receiver, descriptor, value); return value; }

function _classStaticPrivateFieldSpecGet(receiver, classConstructor, descriptor) { _classCheckPrivateStaticAccess(receiver, classConstructor); _classCheckPrivateStaticFieldDescriptor(descriptor, "get"); return _classApplyDescriptorGet(receiver, descriptor); }

function _classCheckPrivateStaticFieldDescriptor(descriptor, action) { if (descriptor === undefined) { throw new TypeError("attempted to " + action + " private static field before its declaration"); } }

function _classCheckPrivateStaticAccess(receiver, classConstructor) { if (receiver !== classConstructor) { throw new TypeError("Private static access of wrong provenance"); } }

function _classPrivateFieldInitSpec(obj, privateMap, value) { _checkPrivateRedeclaration(obj, privateMap); privateMap.set(obj, value); }

function _checkPrivateRedeclaration(obj, privateCollection) { if (privateCollection.has(obj)) { throw new TypeError("Cannot initialize the same private elements twice on an object"); } }

function _classPrivateFieldGet(receiver, privateMap) { var descriptor = _classExtractFieldDescriptor(receiver, privateMap, "get"); return _classApplyDescriptorGet(receiver, descriptor); }

function _classApplyDescriptorGet(receiver, descriptor) { if (descriptor.get) { return descriptor.get.call(receiver); } return descriptor.value; }

function _classPrivateFieldSet(receiver, privateMap, value) { var descriptor = _classExtractFieldDescriptor(receiver, privateMap, "set"); _classApplyDescriptorSet(receiver, descriptor, value); return value; }

function _classExtractFieldDescriptor(receiver, privateMap, action) { if (!privateMap.has(receiver)) { throw new TypeError("attempted to " + action + " private field on non-instance"); } return privateMap.get(receiver); }

function _classApplyDescriptorSet(receiver, descriptor, value) { if (descriptor.set) { descriptor.set.call(receiver, value); } else { if (!descriptor.writable) { throw new TypeError("attempted to set read only private field"); } descriptor.value = value; } }

const path = require("path");

const ACTIONS = {
  GET_VERSION: "GET_VERSION",
  GET_TYPES_INFO: "GET_TYPES_INFO",
  GET_VISITOR_KEYS: "GET_VISITOR_KEYS",
  GET_TOKEN_LABELS: "GET_TOKEN_LABELS",
  MAYBE_PARSE: "MAYBE_PARSE",
  MAYBE_PARSE_SYNC: "MAYBE_PARSE_SYNC"
};

var _send = new WeakMap();

var _vCache = new WeakMap();

var _tiCache = new WeakMap();

var _vkCache = new WeakMap();

var _tlCache = new WeakMap();

class Client {
  constructor(send) {
    _classPrivateFieldInitSpec(this, _send, {
      writable: true,
      value: void 0
    });

    _classPrivateFieldInitSpec(this, _vCache, {
      writable: true,
      value: void 0
    });

    _classPrivateFieldInitSpec(this, _tiCache, {
      writable: true,
      value: void 0
    });

    _classPrivateFieldInitSpec(this, _vkCache, {
      writable: true,
      value: void 0
    });

    _classPrivateFieldInitSpec(this, _tlCache, {
      writable: true,
      value: void 0
    });

    _classPrivateFieldSet(this, _send, send);
  }

  getVersion() {
    var _classPrivateFieldGet2;

    return (_classPrivateFieldGet2 = _classPrivateFieldGet(this, _vCache)) != null ? _classPrivateFieldGet2 : _classPrivateFieldSet(this, _vCache, _classPrivateFieldGet(this, _send).call(this, ACTIONS.GET_VERSION, undefined));
  }

  getTypesInfo() {
    var _classPrivateFieldGet3;

    return (_classPrivateFieldGet3 = _classPrivateFieldGet(this, _tiCache)) != null ? _classPrivateFieldGet3 : _classPrivateFieldSet(this, _tiCache, _classPrivateFieldGet(this, _send).call(this, ACTIONS.GET_TYPES_INFO, undefined));
  }

  getVisitorKeys() {
    var _classPrivateFieldGet4;

    return (_classPrivateFieldGet4 = _classPrivateFieldGet(this, _vkCache)) != null ? _classPrivateFieldGet4 : _classPrivateFieldSet(this, _vkCache, _classPrivateFieldGet(this, _send).call(this, ACTIONS.GET_VISITOR_KEYS, undefined));
  }

  getTokLabels() {
    var _classPrivateFieldGet5;

    return (_classPrivateFieldGet5 = _classPrivateFieldGet(this, _tlCache)) != null ? _classPrivateFieldGet5 : _classPrivateFieldSet(this, _tlCache, _classPrivateFieldGet(this, _send).call(this, ACTIONS.GET_TOKEN_LABELS, undefined));
  }

  maybeParse(code, options) {
    return _classPrivateFieldGet(this, _send).call(this, ACTIONS.MAYBE_PARSE, {
      code,
      options
    });
  }

}

exports.WorkerClient = (_worker = new WeakMap(), (_class = class WorkerClient extends Client {
  constructor() {
    super((action, payload) => {
      const signal = new Int32Array(new SharedArrayBuffer(8));
      const subChannel = new (_classStaticPrivateFieldSpecGet(WorkerClient, _class, _worker_threads).MessageChannel)();

      _classPrivateFieldGet(this, _worker).postMessage({
        signal,
        port: subChannel.port1,
        action,
        payload
      }, [subChannel.port1]);

      Atomics.wait(signal, 0, 0);

      const {
        message
      } = _classStaticPrivateFieldSpecGet(WorkerClient, _class, _worker_threads).receiveMessageOnPort(subChannel.port2);

      if (message.error) throw Object.assign(message.error, message.errorData);else return message.result;
    });

    _classPrivateFieldInitSpec(this, _worker, {
      writable: true,
      value: new (_classStaticPrivateFieldSpecGet(WorkerClient, _class, _worker_threads).Worker)(path.resolve(__dirname, "../lib/worker/index.cjs"), {
        env: _classStaticPrivateFieldSpecGet(WorkerClient, _class, _worker_threads).SHARE_ENV
      })
    });

    _classPrivateFieldGet(this, _worker).unref();
  }

}, _worker_threads = {
  get: _get_worker_threads,
  set: void 0
}, _worker_threads_cache = {
  writable: true,
  value: void 0
}, _class));

function _get_worker_threads() {
  var _classStaticPrivateFi2;

  return (_classStaticPrivateFi2 = _classStaticPrivateFieldSpecGet(_class, _class, _worker_threads_cache)) != null ? _classStaticPrivateFi2 : _classStaticPrivateFieldSpecSet(_class, _class, _worker_threads_cache, require("worker_threads"));
}

{
  var _class2, _handleMessage;

  exports.LocalClient = (_class2 = class LocalClient extends Client {
    constructor() {
      var _classStaticPrivateFi;

      (_classStaticPrivateFi = _classStaticPrivateFieldSpecGet(LocalClient, _class2, _handleMessage)) != null ? _classStaticPrivateFi : _classStaticPrivateFieldSpecSet(LocalClient, _class2, _handleMessage, require("./worker/handle-message.cjs"));
      super((action, payload) => {
        return _classStaticPrivateFieldSpecGet(LocalClient, _class2, _handleMessage).call(LocalClient, action === ACTIONS.MAYBE_PARSE ? ACTIONS.MAYBE_PARSE_SYNC : action, payload);
      });
    }

  }, _handleMessage = {
    writable: true,
    value: void 0
  }, _class2);
}