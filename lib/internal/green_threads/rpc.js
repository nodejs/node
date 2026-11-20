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
// AbortSignals, as host objects: it writes the descriptors that
// exportHostObjects() returns for them ahead of the value, and imports them
// with importHostObjects() before reading the value. Objects are only
// exported once their message has been serialized.

const {
  AggregateError,
  ArrayIsArray,
  ArrayPrototypePush,
  Error,
  EvalError,
  FunctionPrototype,
  FunctionPrototypeCall,
  ObjectDefineProperty,
  ObjectGetOwnPropertyDescriptor,
  ObjectGetOwnPropertyNames,
  ObjectGetPrototypeOf,
  ObjectKeys,
  ObjectPrototype,
  ObjectPrototypeToString,
  PromisePrototypeThen,
  PromiseReject,
  PromiseResolve,
  PromiseWithResolvers,
  RangeError,
  ReferenceError,
  ReflectApply,
  ReflectConstruct,
  SafeFinalizationRegistry,
  SafeMap,
  SafeSet,
  SafeWeakRef,
  String,
  Symbol,
  SymbolDispose,
  SyntaxError,
  TypeError,
  URIError,
} = primordials;

// Message types. Messages are arrays that start with their type.
const kCall = 0;     // [kCall, callId, exportId, method or null, args]
const kResolve = 1;  // [kResolve, callId, value]
const kReject = 2;   // [kReject, callId, encoded error]
const kRelease = 3;  // [kRelease, exportId, count]
const kAbort = 4;    // [kAbort, exportId, encoded reason]
const kExports = 5;  // [kExports, value]: a green thread's module exports

// Kinds of host objects. Their descriptors are arrays that start with their
// kind.
const kTarget = 0;  // [kTarget, exportId, isFunction, methodNames]
const kReturn = 1;  // [kReturn, exportId]
const kSignal = 2;  // [kSignal, exportId, aborted, encoded reason]

// Thrown values. Errors are sent with their constructor, the nearest of
// those in errorConstructors, and all their data properties, like workers'
// errors (see internal/error_serdes), since structured clone drops all but a
// few of them. DOMExceptions, which structured clone doesn't support, are
// sent with what they consist of. Must match src/node_green_thread.cc.
const kErrorValue = 0;         // [kErrorValue, value]
const kErrorDOMException = 1;  // [kErrorDOMException, message, name, stack]
const kErrorObject = 2;        // [kErrorObject, constructor, properties]
const kErrorString = 3;        // [kErrorString, string]: not cloneable

const errorConstructors = {
  __proto__: null,
  AggregateError,
  Error,
  EvalError,
  RangeError,
  ReferenceError,
  SyntaxError,
  TypeError,
  URIError,
};

function isErrorObject(value) {
  try {
    return typeof value === 'object' && value !== null &&
      ObjectPrototypeToString(value) === '[object Error]';
  } catch {
    return false;
  }
}

function errorConstructorName(error) {
  for (let object = error; object !== null;
    object = ObjectGetPrototypeOf(object)) {
    const constructor =
      ObjectGetOwnPropertyDescriptor(object, 'constructor')?.value;
    if (typeof constructor !== 'function') continue;
    const name = ObjectGetOwnPropertyDescriptor(constructor, 'name')?.value;
    if (typeof name === 'string' && errorConstructors[name] !== undefined) {
      return name;
    }
  }
  return 'Error';
}

// The data properties of `error`, own and inherited, by name, as
// { value, writable, enumerable, configurable, thrown }. Getters are called.
// `cause`, and the elements of `errors` (as of AggregateErrors), are encoded
// with encodeThrown(), which `thrown` says. With `minimal`, only those with
// primitive values, which can always be cloned.
function errorProperties(error, minimal, seen) {
  const properties = { __proto__: null };
  const chain = [];
  for (let object = error; object !== null;
    object = ObjectGetPrototypeOf(object)) {
    ArrayPrototypePush(chain, object);
  }
  // From the prototypes to the error itself, so that own properties win.
  for (let i = chain.length - 1; i >= 0; i--) {
    const names = ObjectGetOwnPropertyNames(chain[i]);
    for (let j = 0; j < names.length; j++) {
      const name = names[j];
      let descriptor;
      try {
        descriptor = ObjectGetOwnPropertyDescriptor(chain[i], name);
        if (descriptor.get !== undefined && name !== '__proto__') {
          descriptor = {
            __proto__: null,
            value: FunctionPrototypeCall(descriptor.get, error),
            writable: true,
            enumerable: descriptor.enumerable,
            configurable: true,
          };
        }
      } catch {
        continue;
      }
      if (!('value' in descriptor)) continue;
      let { value } = descriptor;
      let thrown = false;
      if (name === 'cause') {
        value = encodeThrown(value, minimal, seen);
        thrown = true;
      } else if (name === 'errors' && ArrayIsArray(value)) {
        const errors = [];
        for (let k = 0; k < value.length; k++) {
          ArrayPrototypePush(errors, encodeThrown(value[k], minimal, seen));
        }
        value = errors;
        thrown = true;
      } else if (typeof value === 'function' || typeof value === 'symbol' ||
                 (minimal && typeof value === 'object' && value !== null)) {
        continue;
      }
      properties[name] = {
        __proto__: null,
        value,
        writable: !!descriptor.writable,
        enumerable: !!descriptor.enumerable,
        configurable: !!descriptor.configurable,
        thrown,
      };
    }
  }
  return properties;
}

function toStringOrDefault(value) {
  try {
    return String(value);
  } catch {
    return 'Error';
  }
}

/**
 * Encodes a thrown value so that it can be cloned, keeping all of an
 * error's data properties. With `minimal`, the result can always be
 * cloned, but only keeps those with primitive values.
 * @param {any} value
 * @param {boolean} [minimal]
 * @returns {any[]}
 */
function encodeThrown(value, minimal = false, seen = new SafeSet()) {
  if (typeof value === 'symbol' || typeof value === 'function') {
    return [kErrorString, toStringOrDefault(value)];
  }
  if (!isErrorObject(value)) {
    if (minimal && typeof value === 'object' && value !== null) {
      return [kErrorString, toStringOrDefault(value)];
    }
    return [kErrorValue, value];
  }
  if (seen.has(value)) return [kErrorValue, undefined];
  seen.add(value);
  try {
    return [kErrorObject, errorConstructorName(value),
            errorProperties(value, minimal, seen)];
  } catch {
    return [kErrorString, toStringOrDefault(value)];
  }
}

// The inverse of encodeThrown(). Errors are created with their constructor,
// so that they are real errors, and get the properties that they don't
// inherit already.
function decodeThrown(encoded) {
  switch (encoded?.[0]) {
    case kErrorValue:
    case kErrorString:
      return encoded[1];
    case kErrorObject: {
      const constructor = errorConstructors[encoded[1]] ?? Error;
      const error = ReflectConstruct(constructor,
                                     constructor === AggregateError ? [[]] : []);
      const properties = encoded[2];
      const names = ObjectKeys(properties);
      for (let i = 0; i < names.length; i++) {
        const name = names[i];
        const descriptor = properties[name];
        let { value } = descriptor;
        if (descriptor.thrown) {
          if (ArrayIsArray(value) && name === 'errors') {
            const errors = [];
            for (let k = 0; k < value.length; k++) {
              ArrayPrototypePush(errors, decodeThrown(value[k]));
            }
            value = errors;
          } else {
            value = decodeThrown(value);
          }
        }
        const own = ObjectGetOwnPropertyDescriptor(error, name);
        if (own === undefined && !descriptor.thrown && error[name] === value) {
          continue;
        }
        try {
          ObjectDefineProperty(error, name, {
            __proto__: null,
            value,
            writable: !!descriptor.writable,
            enumerable: !!descriptor.enumerable,
            configurable: !!descriptor.configurable,
          });
        } catch {
          // Not configurable.
        }
      }
      return error;
    }
  }
  return encoded;
}

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

  function encodeError(error, minimal = false) {
    if (error instanceof DOMException) {
      return [kErrorDOMException, error.message, error.name, error.stack];
    }
    return encodeThrown(error, minimal);
  }

  function decodeError(encoded) {
    if (encoded?.[0] === kErrorDOMException) {
      const error = new DOMException(encoded[1], encoded[2]);
      ObjectDefineProperty(error, 'stack', {
        __proto__: null,
        value: encoded[3],
        writable: true,
        configurable: true,
      });
      return error;
    }
    return decodeThrown(encoded);
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
    // Calls from the other side that haven't settled yet.
    #activeCalls = 0;
    #nextCallId = 0;

    // Signals whose abort arrived before they did. Never needed for signals
    // that were aborted before they were passed, which carry their reason.
    #pendingAborts = new SafeMap();
    // Imported signals that haven't aborted yet, by ID. They are kept until
    // they abort, since their listeners may be all that refers to them:
    // Node.js doesn't keep signals alive for their listeners.
    #heldSignals = new SafeMap();

    /**
     * @param {(message: any[], isCall: boolean, context?: number) => void} send
     *   Serializes and sends `message`. Throws if it can't be serialized.
     */
    constructor(send) {
      this.#send = send;
    }

    /**
     * Called with the value that the other side sent with sendExports().
     * @type {((value: any) => void) | null}
     */
    onexports = null;

    /**
     * Sends `value`, wrapped like the env, to the other side's onexports.
     * Throws if it can't be serialized.
     * @param {any} value
     */
    sendExports(value) {
      if (this.#closed !== null) return;
      this.#send([kExports, this.wrapEnv(value)], false);
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

    // Called by C++ once a value has been serialized, with the host objects
    // in it, in order. Returns [descriptors, commit]: nothing is exported
    // until C++ calls commit(), which it does once the message can't fail
    // anymore, so that a message that isn't sent keeps nothing alive. Throws,
    // without exporting anything, if an object can't be passed.
    exportHostObjects(objects) {
      const descriptors = [];
      // Exports that the message adds, by value: { id, value, methods }.
      const added = new SafeMap();
      // Export ID => how many references the message passes.
      const refs = new SafeMap();
      // The IDs of signals that are passed as not aborted.
      const listenedSignals = new SafeSet();
      // The export of `value`, existing or added: { id, methods }.
      const exportOf = (value, getMethods) => {
        let entry = added.get(value);
        let id = entry?.id;
        if (entry === undefined) {
          id = this.#exportIds.get(value);
          entry = this.#exports.get(id);
        }
        if (entry === undefined) {
          id = ++this.#nextExportId;
          entry = { id, value, methods: getMethods() };
          added.set(value, entry);
        }
        refs.set(id, (refs.get(id) ?? 0) + 1);
        return { id, methods: entry.methods };
      };
      for (let i = 0; i < objects.length; i++) {
        const object = objects[i];
        let descriptor;
        if (isAbortSignal(object)) {
          // Signals that are aborted are passed with their reason.
          const { id } = exportOf(object, () => null);
          if (!object.aborted) listenedSignals.add(id);
          descriptor = object.aborted ?
            [kSignal, id, true, encodeError(object.reason)] :
            [kSignal, id, false, null];
        } else {
          const target =
            object instanceof FunctionReference ? object.fn : object;
          const stub = target[brand];
          if (stub instanceof Stub && stub.session === this) {
            if (stub.disposed) throw errors.disposed();
            descriptor = [kReturn, stub.id];
          } else if (typeof target === 'function') {
            const { id } = exportOf(target, () => null);
            descriptor = [kTarget, id, true, []];
          } else {
            const { id, methods } =
              exportOf(target, () => new SafeSet(methodNames(target)));
            const names = [];
            for (const name of methods) ArrayPrototypePush(names, name);
            descriptor = [kTarget, id, false, names];
          }
        }
        ArrayPrototypePush(descriptors, descriptor);
      }
      const commit = () => {
        if (this.#closed !== null) return;
        for (const { id, value, methods } of added.values()) {
          const entry = { value, refs: 0, methods, removeListener: null };
          this.#exports.set(id, entry);
          this.#exportIds.set(value, id);
          if (listenedSignals.has(id)) this.#listenForAbort(id, entry, value);
        }
        for (const { 0: id, 1: count } of refs) {
          const entry = this.#exports.get(id);
          if (entry !== undefined) entry.refs += count;
        }
      };
      return [descriptors, commit];
    }

    // Tells the other side when `signal`, exported as `id`, aborts. If it was
    // aborted while the message that passes it was serialized, that's now,
    // before the message, which the other side handles.
    #listenForAbort(id, entry, signal) {
      const send = () => {
        entry.removeListener = null;
        this.#trySend([kAbort, id, encodeError(signal.reason)], false);
      };
      if (signal.aborted) return send();
      signal.addEventListener('abort', send, { once: true });
      entry.removeListener = () => {
        signal.removeEventListener('abort', send);
      };
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
            if (!signal.aborted) this.#heldSignals.set(descriptor[1], signal);
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
        case kExports:
          this.onexports?.(message[1]);
          break;
        case kAbort: {
          const reason = decodeError(message[2]);
          const signal = this.#heldSignals.get(message[1]);
          if (signal === undefined) {
            this.#pendingAborts.set(message[1], reason);
          } else {
            // Released once it is collected.
            this.#heldSignals.delete(message[1]);
            signal[kController].abort(reason);
          }
          break;
        }
      }
    }

    /**
     * The number of calls from the other side that haven't settled.
     * @returns {number}
     */
    get activeCalls() {
      return this.#activeCalls;
    }

    #onCall(callId, id, method, args, context) {
      const entry = this.#exports.get(id);
      this.#activeCalls++;
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
        this.#activeCalls--;
        try {
          this.#trySend([kResolve, callId, wrapValue(value)], false, context);
        } catch (error) {
          this.#sendError(callId, error, context);
        }
      }, (error) => {
        this.#activeCalls--;
        this.#sendError(callId, error, context);
      });
    }

    #sendError(callId, error, context) {
      try {
        this.#trySend([kReject, callId, encodeError(error)], false, context);
      } catch {
        // Not cloneable.
        this.#trySend([kReject, callId, encodeError(error, true)], false,
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
      this.#heldSignals.clear();
    }
  }

  return { RpcTarget, RpcSession, encodeError, decodeError };
}

module.exports = { createRpc };
