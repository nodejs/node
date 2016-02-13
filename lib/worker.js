'use strict';

if (!process.features.experimental_workers) {
  throw new Error('Experimental workers are not enabled');
}

const util = require('util');
const assert = require('assert');
const EventEmitter = require('events');
const WorkerContextBinding = process.binding('worker_context');
const JSONStringify = function(value) {
  if (value === undefined) value = null;
  return JSON.stringify(value);
};
const JSONParse = JSON.parse;
const EMPTY_ARRAY = [];

const workerContextSymbol = Symbol('workerContext');
const installEventsSymbol = Symbol('installEvents');
const checkAliveSymbol = Symbol('checkAlive');
const initSymbol = WorkerContextBinding.initSymbol;

const builtinErrorTypes = new Map([
  Error, SyntaxError, RangeError, URIError, TypeError, EvalError, ReferenceError
].map(function(Type) {
  return [Type.name, Type];
}));

const Worker = WorkerContextBinding.JsConstructor;
util.inherits(Worker, EventEmitter);

Worker.prototype[initSymbol] = function(entryModulePath, options) {
  if (typeof entryModulePath !== 'string')
    throw new TypeError('entryModulePath must be a string');
  EventEmitter.call(this);
  options = Object(options);
  const keepAlive =
      options.keepAlive === undefined ? true : !!options.keepAlive;
  const evalCode = !!options.eval;
  const userData = JSONStringify(options.data);
  this[workerContextSymbol] =
      new WorkerContextBinding.WorkerContext(entryModulePath, {
        keepAlive: keepAlive,
        userData: userData,
        eval: evalCode
      });
  this[installEventsSymbol]();
};

Worker.prototype[installEventsSymbol] = function() {
  const workerObject = this;
  const workerContext = this[workerContextSymbol];

  const onerror = function(payload) {
    var ErrorConstructor = builtinErrorTypes.get(payload.builtinType);
    if (typeof ErrorConstructor !== 'function')
      ErrorConstructor = Error;
    const error = new ErrorConstructor(payload.message);
    error.stack = payload.stack;
    util._extend(error, payload.additionalProperties);
    workerObject.emit('error', error);
  };

  workerContext._onexit = function(exitCode) {
    workerObject[workerContextSymbol] = null;
    workerObject.emit('exit', exitCode);
  };

  workerContext._onmessage = function(payload, messageType) {
    payload = JSONParse(payload);
    switch (messageType) {
      case WorkerContextBinding.USER:
        return workerObject.emit('message', payload);
      case WorkerContextBinding.EXCEPTION:
        return onerror(payload);
      default:
        assert.fail('unreachable');
    }
  };
};

Worker.prototype[checkAliveSymbol] = function() {
  if (!this[workerContextSymbol])
    throw new RangeError('this worker has been terminated');
};

Worker.prototype.postMessage = function(payload) {
  this[checkAliveSymbol]();
  this[workerContextSymbol].postMessage(JSONStringify(payload),
                                        EMPTY_ARRAY,
                                        WorkerContextBinding.USER);
};

Worker.prototype.terminate = function(callback) {
  this[checkAliveSymbol]();
  const context = this[workerContextSymbol];
  this[workerContextSymbol] = null;
  if (typeof callback === 'function') {
    this.once('exit', function(exitCode) {
      callback(null, exitCode);
    });
  }
  context.terminate();
};

Worker.prototype.ref = function() {
  this[checkAliveSymbol]();
  this[workerContextSymbol].ref();
};

Worker.prototype.unref = function() {
  this[checkAliveSymbol]();
  this[workerContextSymbol].unref();
};

if (process.isWorkerThread) {
  const postMessage = function(payload, transferList, type) {
    if (!Array.isArray(transferList))
      throw new TypeError('transferList must be an array');

    WorkerContextBinding.workerWrapper._postMessage(JSONStringify(payload),
                                                    transferList,
                                                    type);
  };
  const workerFatalError = function(er) {
    const defaultStack = null;
    const defaultMessage = '[toString() conversion failed]';
    const defaultBuiltinType = 'Error';

    var message = defaultMessage;
    var builtinType = defaultBuiltinType;
    var stack = defaultStack;
    var additionalProperties = {};

    // As this is a fatal error handler we cannot throw any new errors here
    // but should still try to extract as much info as possible from the
    // cause.
    if (er instanceof Error) {
      // .name can be a getter that throws.
      try {
        builtinType = er.name;
      } catch (ignore) {}

      if (typeof builtinType !== 'string')
        builtinType = defaultBuiltinType;

      // .stack can be a getter that throws.
      try {
        stack = er.stack;
      } catch (ignore) {}

      if (typeof stack !== 'string')
        stack = defaultStack;

      try {
        // Get inherited enumerable properties.
        // .name, .stack and .message are all non-enumerable
        for (var key in er)
          additionalProperties[key] = er[key];
        // The message delivery must always succeed, otherwise the real cause
        // of this fatal error is masked.
        JSONStringify(additionalProperties);
      } catch (e) {
        additionalProperties = {};
      }
    }

    try {
      // .message can be a getter that throws or the call to toString may fail.
      if (er instanceof Error) {
        message = er.message;
        if (typeof message !== 'string')
          message = '' + er;
      } else {
        message = '' + er;
      }
    } catch (e) {
      message = defaultMessage;
    }

    postMessage({
      message: message,
      stack: stack,
      additionalProperties: additionalProperties,
      builtinType: builtinType
    }, EMPTY_ARRAY, WorkerContextBinding.EXCEPTION);
  };

  util._extend(Worker, EventEmitter.prototype);
  EventEmitter.call(Worker);

  WorkerContextBinding.workerWrapper._onmessage =
      function(payload, messageType) {
        payload = JSONParse(payload);
        switch (messageType) {
          case WorkerContextBinding.USER:
            return Worker.emit('message', payload);
          default:
            assert.fail('unreachable');
        }
      };

  Worker.postMessage = function(payload) {
    postMessage(payload, EMPTY_ARRAY, WorkerContextBinding.USER);
  };

  Object.defineProperty(Worker, '_workerFatalError', {
    configurable: true,
    writable: false,
    enumerable: false,
    value: workerFatalError
  });
}


module.exports = Worker;
