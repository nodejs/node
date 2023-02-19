'use strict';
const {
  ArrayPrototypePush,
  ArrayPrototypeSlice,
  Error,
  FunctionPrototypeCall,
  ObjectDefineProperty,
  ObjectGetOwnPropertyDescriptor,
  ObjectGetPrototypeOf,
  Proxy,
  ReflectApply,
  ReflectConstruct,
  ReflectGet,
  SafeMap,
} = primordials;
const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
  },
} = require('internal/errors');
const { kEmptyObject } = require('internal/util');
const {
  validateBoolean,
  validateFunction,
  validateInteger,
  validateObject,
} = require('internal/validators');

function kDefaultFunction() {}

class MockFunctionContext {
  #calls;
  #mocks;
  #implementation;
  #restore;
  #times;

  constructor(implementation, restore, times) {
    this.#calls = [];
    this.#mocks = new SafeMap();
    this.#implementation = implementation;
    this.#restore = restore;
    this.#times = times;
  }

  get calls() {
    return ArrayPrototypeSlice(this.#calls, 0);
  }

  callCount() {
    return this.#calls.length;
  }

  mockImplementation(implementation) {
    validateFunction(implementation, 'implementation');
    this.#implementation = implementation;
  }

  mockImplementationOnce(implementation, onCall) {
    validateFunction(implementation, 'implementation');
    const nextCall = this.#calls.length;
    const call = onCall ?? nextCall;
    validateInteger(call, 'onCall', nextCall);
    this.#mocks.set(call, implementation);
  }

  restore() {
    const { descriptor, object, original, methodName } = this.#restore;

    if (typeof methodName === 'string') {
      // This is an object method spy.
      ObjectDefineProperty(object, methodName, descriptor);
    } else {
      // This is a bare function spy. There isn't much to do here but make
      // the mock call the original function.
      this.#implementation = original;
    }
  }

  resetCalls() {
    this.#calls = [];
  }

  trackCall(call) {
    ArrayPrototypePush(this.#calls, call);
  }

  nextImpl() {
    const nextCall = this.#calls.length;
    const mock = this.#mocks.get(nextCall);
    const impl = mock ?? this.#implementation;

    if (nextCall + 1 === this.#times) {
      this.restore();
    }

    this.#mocks.delete(nextCall);
    return impl;
  }
}

const { nextImpl, restore, trackCall } = MockFunctionContext.prototype;
delete MockFunctionContext.prototype.trackCall;
delete MockFunctionContext.prototype.nextImpl;

class MockTracker {
  #mocks = [];

  fn(
    original = function() {},
    implementation = original,
    options = kEmptyObject,
  ) {
    if (original !== null && typeof original === 'object') {
      options = original;
      original = function() {};
      implementation = original;
    } else if (implementation !== null && typeof implementation === 'object') {
      options = implementation;
      implementation = original;
    }

    validateFunction(original, 'original');
    validateFunction(implementation, 'implementation');
    validateObject(options, 'options');
    const { times = Infinity } = options;
    validateTimes(times, 'options.times');
    const ctx = new MockFunctionContext(implementation, { original }, times);
    return this.#setupMock(ctx, original);
  }

  method(
    objectOrFunction,
    methodName,
    implementation = kDefaultFunction,
    options = kEmptyObject,
  ) {
    validateStringOrSymbol(methodName, 'methodName');
    if (typeof objectOrFunction !== 'function') {
      validateObject(objectOrFunction, 'object');
    }

    if (implementation !== null && typeof implementation === 'object') {
      options = implementation;
      implementation = kDefaultFunction;
    }

    validateFunction(implementation, 'implementation');
    validateObject(options, 'options');

    const {
      getter = false,
      setter = false,
      times = Infinity,
    } = options;

    validateBoolean(getter, 'options.getter');
    validateBoolean(setter, 'options.setter');
    validateTimes(times, 'options.times');

    if (setter && getter) {
      throw new ERR_INVALID_ARG_VALUE(
        'options.setter', setter, "cannot be used with 'options.getter'",
      );
    }
    const descriptor = findMethodOnPrototypeChain(objectOrFunction, methodName);

    let original;

    if (getter) {
      original = descriptor?.get;
    } else if (setter) {
      original = descriptor?.set;
    } else {
      original = descriptor?.value;
    }

    if (typeof original !== 'function') {
      throw new ERR_INVALID_ARG_VALUE(
        'methodName', original, 'must be a method',
      );
    }

    const restore = { descriptor, object: objectOrFunction, methodName };
    const impl = implementation === kDefaultFunction ?
      original : implementation;
    const ctx = new MockFunctionContext(impl, restore, times);
    const mock = this.#setupMock(ctx, original);
    const mockDescriptor = {
      __proto__: null,
      configurable: descriptor.configurable,
      enumerable: descriptor.enumerable,
    };

    if (getter) {
      mockDescriptor.get = mock;
      mockDescriptor.set = descriptor.set;
    } else if (setter) {
      mockDescriptor.get = descriptor.get;
      mockDescriptor.set = mock;
    } else {
      mockDescriptor.writable = descriptor.writable;
      mockDescriptor.value = mock;
    }

    ObjectDefineProperty(objectOrFunction, methodName, mockDescriptor);

    return mock;
  }

  getter(
    object,
    methodName,
    implementation = kDefaultFunction,
    options = kEmptyObject,
  ) {
    if (implementation !== null && typeof implementation === 'object') {
      options = implementation;
      implementation = kDefaultFunction;
    } else {
      validateObject(options, 'options');
    }

    const { getter = true } = options;

    if (getter === false) {
      throw new ERR_INVALID_ARG_VALUE(
        'options.getter', getter, 'cannot be false',
      );
    }

    return this.method(object, methodName, implementation, {
      ...options,
      getter,
    });
  }

  setter(
    object,
    methodName,
    implementation = kDefaultFunction,
    options = kEmptyObject,
  ) {
    if (implementation !== null && typeof implementation === 'object') {
      options = implementation;
      implementation = kDefaultFunction;
    } else {
      validateObject(options, 'options');
    }

    const { setter = true } = options;

    if (setter === false) {
      throw new ERR_INVALID_ARG_VALUE(
        'options.setter', setter, 'cannot be false',
      );
    }

    return this.method(object, methodName, implementation, {
      ...options,
      setter,
    });
  }

  reset() {
    this.restoreAll();
    this.#mocks = [];
  }

  restoreAll() {
    for (let i = 0; i < this.#mocks.length; i++) {
      FunctionPrototypeCall(restore, this.#mocks[i]);
    }
  }

  #setupMock(ctx, fnToMatch) {
    const mock = new Proxy(fnToMatch, {
      __proto__: null,
      apply(_fn, thisArg, argList) {
        const fn = FunctionPrototypeCall(nextImpl, ctx);
        let result;
        let error;

        try {
          result = ReflectApply(fn, thisArg, argList);
        } catch (err) {
          error = err;
          throw err;
        } finally {
          FunctionPrototypeCall(trackCall, ctx, {
            arguments: argList,
            error,
            result,
            // eslint-disable-next-line no-restricted-syntax
            stack: new Error(),
            target: undefined,
            this: thisArg,
          });
        }

        return result;
      },
      construct(target, argList, newTarget) {
        const realTarget = FunctionPrototypeCall(nextImpl, ctx);
        let result;
        let error;

        try {
          result = ReflectConstruct(realTarget, argList, newTarget);
        } catch (err) {
          error = err;
          throw err;
        } finally {
          FunctionPrototypeCall(trackCall, ctx, {
            arguments: argList,
            error,
            result,
            // eslint-disable-next-line no-restricted-syntax
            stack: new Error(),
            target,
            this: result,
          });
        }

        return result;
      },
      get(target, property, receiver) {
        if (property === 'mock') {
          return ctx;
        }

        return ReflectGet(target, property, receiver);
      },
    });

    ArrayPrototypePush(this.#mocks, ctx);
    return mock;
  }
}

function validateStringOrSymbol(value, name) {
  if (typeof value !== 'string' && typeof value !== 'symbol') {
    throw new ERR_INVALID_ARG_TYPE(name, ['string', 'symbol'], value);
  }
}

function validateTimes(value, name) {
  if (value === Infinity) {
    return;
  }

  validateInteger(value, name, 1);
}

function findMethodOnPrototypeChain(instance, methodName) {
  let host = instance;
  let descriptor;

  while (host !== null) {
    descriptor = ObjectGetOwnPropertyDescriptor(host, methodName);

    if (descriptor) {
      break;
    }

    host = ObjectGetPrototypeOf(host);
  }

  return descriptor;
}

module.exports = { MockTracker };
