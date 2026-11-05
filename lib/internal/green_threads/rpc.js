'use strict';

// RPC between a green thread and its host. Both sides run this module: the
// host with Node.js' primordials, a green thread with its own (see
// internal/green_threads/runtime/primordials), so it only uses primordials
// that both have, and gets everything else from createRpc().
//
// Each side exports objects that the other side can call: RpcTargets, and
// functions. They are passed as stubs, whose methods return promises. A
// stub passed back to the side that exported it turns into the original
// object again. AbortSignals are passed as signals that abort when the
// original does.
//
// Values are serialized by C++, which treats objects with `brand`, and
// AbortSignals, as host objects: it writes what exportHostObject() returns
// for them ahead of the value, and imports them with importHostObjects()
// before reading the value.

const {
  ArrayIsArray,
  ArrayPrototypePush,
  FunctionPrototype,
  ObjectDefineProperty,
  ObjectGetOwnPropertyDescriptor,
  ObjectGetOwnPropertyNames,
  ObjectGetPrototypeOf,
  ObjectPrototype,
  PromisePrototypeThen,
  PromiseReject,
  PromiseResolve,
  PromiseWithResolvers,
  ReflectApply,
  SafeFinalizationRegistry,
  SafeMap,
  SafeSet,
  SafeWeakRef,
  Symbol,
  SymbolDispose,
} = primordials;

// Message types. Messages are arrays that start with their type.
const kCall = 0;     // [kCall, callId, exportId, method or null, args]
const kResolve = 1;  // [kResolve, callId, value]
const kReject = 2;   // [kReject, callId, encoded error]
const kRelease = 3;  // [kRelease, exportId, count]
const kAbort = 4;    // [kAbort, exportId, encoded reason]

// Kinds of host objects. Their descriptors are arrays that start with their
// kind.
const kTarget = 0;  // [kTarget, exportId, isFunction, methodNames]
const kReturn = 1;  // [kReturn, exportId]
const kSignal = 2;  // [kSignal, exportId, aborted, encoded reason]

// Errors are cloned, except DOMExceptions, which structured clone doesn't
// support, and values that can't be cloned.
const kErrorValue = 0;         // [kErrorValue, value]
const kErrorDOMException = 1;  // [kErrorDOMException, message, name, stack]

/**
 * @param {{
 *   brand: symbol,
 *   AbortController: Function,
 *   AbortSignal: Function,
 *   DOMException: Function,
 *   errors: {
 *     disposed: () => Error,
 *     noSuchMethod: (name: string) => Error,
 *     unknownExport: () => Error,
 *   },
 * }} config
 * @returns {{ RpcTarget: Function, RpcSession: Function }}
 */
function createRpc(config) {
  const {
    brand,
    AbortController,
    AbortSignal,
    DOMException,
    errors,
  } = config;
  const abortSignalPrototype = AbortSignal.prototype;

  /**
   * The base class of objects whose methods the other side can call. Their
   * other properties are not passed.
   */
  class RpcTarget {
    constructor() {
      this[brand] = true;
    }
  }

  // A function passed directly as an argument or result, which V8 doesn't
  // let C++ serialize as a host object itself.
  class FunctionReference {
    constructor(fn) {
      this.fn = fn;
      this[brand] = true;
    }
  }

  function wrapValue(value) {
    return typeof value === 'function' ? new FunctionReference(value) : value;
  }

  function wrapArgs(args) {
    const wrapped = [];
    for (let i = 0; i < args.length; i++) {
      ArrayPrototypePush(wrapped, wrapValue(args[i]));
    }
    return wrapped;
  }

  // The names of the methods of `target` that may be called: those it has
  // or inherits, except from Object.prototype and Function.prototype.
  function methodNames(target) {
    const names = [];
    const seen = new SafeSet(['constructor']);
    for (let object = target;
      object !== null && object !== ObjectPrototype &&
        object !== FunctionPrototype && object !== RpcTarget.prototype;
      object = ObjectGetPrototypeOf(object)) {
      const ownNames = ObjectGetOwnPropertyNames(object);
      for (let i = 0; i < ownNames.length; i++) {
        const name = ownNames[i];
        if (seen.has(name)) continue;
        seen.add(name);
        const descriptor = ObjectGetOwnPropertyDescriptor(object, name);
        if (typeof descriptor.value === 'function') {
          ArrayPrototypePush(names, name);
        }
      }
    }
    return names;
  }

  function encodeError(error) {
    if (error instanceof DOMException) {
      return [kErrorDOMException, error.message, error.name, error.stack];
    }
    return [kErrorValue, error];
  }

  function decodeError(encoded) {
    if (encoded[0] === kErrorDOMException) {
      const error = new DOMException(encoded[1], encoded[2]);
      ObjectDefineProperty(error, 'stack', {
        __proto__: null,
        value: encoded[3],
        writable: true,
        configurable: true,
      });
      return error;
    }
    return encoded[1];
  }

  function isAbortSignal(value) {
    for (let object = value; object !== null;
      object = ObjectGetPrototypeOf(object)) {
      if (object === abortSignalPrototype) return true;
    }
    return false;
  }

  const kController = Symbol('kController');

  // The state of a stub, which `brand` refers to.
  class Stub {
    constructor(session, id) {
      this.session = session;
      this.id = id;
      this.disposed = false;
      this.object = null;
    }
  }

  const stubPrototype = {
    __proto__: null,
    [SymbolDispose]() {
      const stub = this[brand];
      if (stub instanceof Stub) stub.session.dispose(stub);
    },
  };

  function createStub(session, id, isFunction, names) {
    const stub = new Stub(session, id);
    let object;
    if (isFunction) {
      object = function(...args) {
        return session.call(stub, null, args);
      };
      object[SymbolDispose] = stubPrototype[SymbolDispose];
    } else {
      object = { __proto__: stubPrototype };
      for (let i = 0; i < names.length; i++) {
        const name = names[i];
        ObjectDefineProperty(object, name, {
          __proto__: null,
          value: (...args) => session.call(stub, name, args),
          writable: false,
          enumerable: true,
          configurable: false,
        });
      }
    }
    object[brand] = stub;
    stub.object = object;
    return object;
  }

  /**
   * One side of the connection between a green thread and its host.
   */
  class RpcSession {
    // Sends a message. `isCall` is true for calls, whose result the other
    // side sends back with `context`, the context of the call on this side.
    #send;
    #closed = null;

    // Objects that this side exported, by ID: { value, refs, methods,
    // removeListener }. The other side holds `refs` references to each.
    #exports = new SafeMap();
    #exportIds = new SafeMap();
    #nextExportId = 0;

    // Stubs for objects that the other side exported, by ID: { stub,
    // count }, where `stub` is a SafeWeakRef and the other side has been told
    // about `count` references.
    #imports = new SafeMap();
    #registry = new SafeFinalizationRegistry((id) => this.#onCollected(id));

    // Calls made by this side, by ID.
    #pending = new SafeMap();
    #nextCallId = 0;

    // Signals whose abort arrived before they did. Never needed for signals
    // that were aborted before they were passed, which carry their reason.
    #pendingAborts = new SafeMap();

    /**
     * @param {(message: any[], isCall: boolean, context?: number) => void} send
     *   Serializes and sends `message`. Throws if it can't be serialized.
     */
    constructor(send) {
      this.#send = send;
    }

    get abortSignalPrototype() {
      return abortSignalPrototype;
    }

    /**
     * Wraps a value so that it can be serialized with this session's hooks:
     * functions at the top level, or anywhere in plain objects and arrays,
     * are passed as stubs.
     * @param {any} value
     * @returns {any}
     */
    wrapEnv(value) {
      if (typeof value === 'function') return new FunctionReference(value);
      if (typeof value !== 'object' || value === null || value[brand] ||
          isAbortSignal(value)) {
        return value;
      }
      if (ArrayIsArray(value)) {
        const result = [];
        for (let i = 0; i < value.length; i++) {
          ArrayPrototypePush(result, this.wrapEnv(value[i]));
        }
        return result;
      }
      const prototype = ObjectGetPrototypeOf(value);
      if (prototype !== ObjectPrototype && prototype !== null) return value;
      const result = { __proto__: null };
      const names = ObjectGetOwnPropertyNames(value);
      for (let i = 0; i < names.length; i++) {
        result[names[i]] = this.wrapEnv(value[names[i]]);
      }
      return result;
    }

    // Called by C++ while serializing.
    exportHostObject(object) {
      if (isAbortSignal(object)) return this.#exportSignal(object);
      const target = object instanceof FunctionReference ? object.fn : object;
      const stub = target[brand];
      if (stub instanceof Stub && stub.session === this) {
        if (stub.disposed) throw errors.disposed();
        return [kReturn, stub.id];
      }
      const isFunction = typeof target === 'function';
      let id = this.#exportIds.get(target);
      if (id === undefined) {
        id = ++this.#nextExportId;
        this.#exportIds.set(target, id);
        this.#exports.set(id, {
          value: target,
          refs: 0,
          methods: isFunction ? null : new SafeSet(methodNames(target)),
          removeListener: null,
        });
      }
      const entry = this.#exports.get(id);
      entry.refs++;
      const names = [];
      if (!isFunction) {
        for (const name of entry.methods) ArrayPrototypePush(names, name);
      }
      return [kTarget, id, isFunction, names];
    }

    #exportSignal(signal) {
      let id = this.#exportIds.get(signal);
      if (id === undefined) {
        id = ++this.#nextExportId;
        this.#exportIds.set(signal, id);
        const entry = {
          value: signal,
          refs: 0,
          methods: null,
          removeListener: null,
        };
        this.#exports.set(id, entry);
        if (!signal.aborted) {
          const listener = () => {
            entry.removeListener = null;
            this.#trySend([kAbort, id, encodeError(signal.reason)], false);
          };
          signal.addEventListener('abort', listener, { once: true });
          entry.removeListener = () => {
            signal.removeEventListener('abort', listener);
          };
        }
      }
      this.#exports.get(id).refs++;
      if (signal.aborted) {
        return [kSignal, id, true, encodeError(signal.reason)];
      }
      return [kSignal, id, false, null];
    }

    // Called by C++ before deserializing a value, with the descriptors of
    // the host objects in it.
    importHostObjects(descriptors) {
      const objects = [];
      for (let i = 0; i < descriptors.length; i++) {
        ArrayPrototypePush(objects, this.#importHostObject(descriptors[i]));
      }
      return objects;
    }

    #importHostObject(descriptor) {
      switch (descriptor[0]) {
        case kReturn: {
          const entry = this.#exports.get(descriptor[1]);
          if (entry === undefined) throw errors.unknownExport();
          return entry.value;
        }
        case kTarget:
          return this.#import(descriptor[1], () => {
            return createStub(this, descriptor[1], descriptor[2],
                              descriptor[3]);
          });
        case kSignal:
          return this.#import(descriptor[1], () => {
            const controller = new AbortController();
            if (descriptor[2]) {
              controller.abort(decodeError(descriptor[3]));
            } else if (this.#pendingAborts.has(descriptor[1])) {
              controller.abort(this.#pendingAborts.get(descriptor[1]));
              this.#pendingAborts.delete(descriptor[1]);
            }
            const { signal } = controller;
            signal[kController] = controller;
            return signal;
          });
      }
      throw errors.unknownExport();
    }

    #import(id, create) {
      let entry = this.#imports.get(id);
      let object = entry?.stub.deref();
      if (object === undefined) {
        object = create();
        if (entry === undefined) {
          entry = { stub: null, count: 0 };
          this.#imports.set(id, entry);
        }
        entry.stub = new SafeWeakRef(object);
        this.#registry.register(object, id, object);
      }
      entry.count++;
      return object;
    }

    #onCollected(id) {
      const entry = this.#imports.get(id);
      // A new stub for the same export may be in use.
      if (entry === undefined || entry.stub.deref() !== undefined) return;
      this.#release(id, entry);
    }

    #release(id, entry) {
      this.#imports.delete(id);
      this.#trySend([kRelease, id, entry.count], false);
    }

    dispose(stub) {
      if (stub.disposed) return;
      stub.disposed = true;
      const entry = this.#imports.get(stub.id);
      if (entry === undefined || entry.stub.deref() !== stub.object) return;
      this.#registry.unregister(stub.object);
      this.#release(stub.id, entry);
    }

    call(stub, method, args) {
      if (this.#closed !== null) return PromiseReject(this.#closed);
      if (stub.disposed) return PromiseReject(errors.disposed());
      const callId = ++this.#nextCallId;
      const pending = PromiseWithResolvers();
      this.#pending.set(callId, pending);
      try {
        this.#send([kCall, callId, stub.id, method, wrapArgs(args)], true);
      } catch (error) {
        this.#pending.delete(callId);
        pending.reject(error);
      }
      return pending.promise;
    }

    // Sends a message that nothing waits for. It can't fail to serialize.
    #trySend(message, isCall, context) {
      if (this.#closed !== null) return;
      this.#send(message, isCall, context);
    }

    /**
     * Handles a message from the other side. `context` is passed back with
     * the result of a call.
     */
    onMessage(message, context) {
      switch (message[0]) {
        case kCall:
          this.#onCall(message[1], message[2], message[3], message[4],
                       context);
          break;
        case kResolve:
        case kReject: {
          const pending = this.#pending.get(message[1]);
          if (pending === undefined) break;
          this.#pending.delete(message[1]);
          if (message[0] === kResolve) {
            pending.resolve(message[2]);
          } else {
            pending.reject(decodeError(message[2]));
          }
          break;
        }
        case kRelease: {
          const entry = this.#exports.get(message[1]);
          if (entry === undefined) break;
          entry.refs -= message[2];
          if (entry.refs > 0) break;
          this.#exports.delete(message[1]);
          this.#exportIds.delete(entry.value);
          entry.removeListener?.();
          break;
        }
        case kAbort: {
          const reason = decodeError(message[2]);
          const signal = this.#imports.get(message[1])?.stub.deref();
          if (signal === undefined) {
            this.#pendingAborts.set(message[1], reason);
          } else {
            signal[kController].abort(reason);
          }
          break;
        }
      }
    }

    #onCall(callId, id, method, args, context) {
      const entry = this.#exports.get(id);
      let result;
      try {
        if (entry === undefined) throw errors.unknownExport();
        const { value } = entry;
        let fn;
        if (method === null) {
          fn = entry.methods === null ? value : undefined;
        } else if (entry.methods?.has(method)) {
          fn = value[method];
        }
        if (typeof fn !== 'function') throw errors.noSuchMethod(method);
        result = PromiseResolve(ReflectApply(fn, value, args));
      } catch (error) {
        result = PromiseReject(error);
      }
      PromisePrototypeThen(result, (value) => {
        try {
          this.#trySend([kResolve, callId, wrapValue(value)], false, context);
        } catch (error) {
          this.#sendError(callId, error, context);
        }
      }, (error) => {
        this.#sendError(callId, error, context);
      });
    }

    #sendError(callId, error, context) {
      try {
        this.#trySend([kReject, callId, encodeError(error)], false, context);
      } catch {
        // Not cloneable.
        let string;
        try {
          string = `${error}`;
        } catch {
          string = 'Error';
        }
        this.#trySend([kReject, callId, [kErrorValue, string]], false,
                      context);
      }
    }

    /**
     * Rejects all pending calls with `error`, and drops all exports. Nothing
     * is sent after this.
     */
    close(error) {
      if (this.#closed !== null) return;
      this.#closed = error;
      for (const { reject } of this.#pending.values()) reject(error);
      this.#pending.clear();
      for (const entry of this.#exports.values()) entry.removeListener?.();
      this.#exports.clear();
      this.#exportIds.clear();
      this.#imports.clear();
    }
  }

  return { RpcTarget, RpcSession };
}

module.exports = { createRpc };
