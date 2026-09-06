'use strict';

const {
  ArrayPrototypeIncludes,
  ArrayPrototypePop,
  ArrayPrototypePush,
  ArrayPrototypeSlice,
  DateNow,
  FunctionPrototypeCall,
  JSONStringify,
  NumberIsFinite,
  ObjectAssign,
  ObjectDefineProperties,
  ObjectDefineProperty,
  ObjectFreeze,
  ObjectGetOwnPropertySymbols,
  ObjectGetPrototypeOf,
  ObjectKeys,
  ObjectPrototypeHasOwnProperty,
  ObjectPrototypePropertyIsEnumerable,
  ObjectSetPrototypeOf,
  ReflectOwnKeys,
  SafeMap,
  SafeSet,
  SafeWeakMap,
  SafeWeakRef,
  String,
  StringPrototypeCharCodeAt,
  Symbol,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_RETURN_VALUE,
    ERR_INVALID_STATE,
    ERR_INVALID_THIS,
  },
} = require('internal/errors');
const {
  validateArray,
  validateBoolean,
  validateFunction,
  validateInteger,
  validateObject,
  validateOneOf,
  validateString,
} = require('internal/validators');
const {
  emitExperimentalWarning,
  isError,
  kEmptyObject,
} = require('internal/util');
const Utf8Stream = require('internal/streams/fast-utf8-stream');
const { channel: diagnosticsChannel } = require('diagnostics_channel');
const EventEmitter = require('events');

// Objects created per logger or per event (levels, bindings, attributes,
// contexts, events, records) must not inherit from Object.prototype, and
// should be in fast properties mode. `{ __proto__: null }` literals and
// ObjectCreate(null) produce dictionary-mode objects, and calling
// ObjectSetPrototypeOf() on a new object is expensive. Instead, these
// objects are created by constructor functions whose prototype is this
// shared, empty, frozen object, whose own prototype is null. Such objects
// are allocated in fast mode and inherit no properties.
//
// Objects created only once (at module load or per provider) use
// ObjectSetPrototypeOf(obj, null) directly, since the cost is paid once.
const kNullPrototype = {};
ObjectSetPrototypeOf(kNullPrototype, null);
ObjectFreeze(kNullPrototype);

function NullPrototypeObject() {}
NullPrototypeObject.prototype = kNullPrototype;

function Level(name, value) {
  this.name = name;
  this.value = value;
}
Level.prototype = kNullPrototype;

// Returns ConsoleProvider's cached serialization of `bindings` (see
// getSerializedBindings()) if `context` is the context of the logger that
// owns them, otherwise undefined. Set in LoggerContext's static block.
let getContextSerializedBindings;

class LoggerContext {
  // Internal state, invisible to providers.
  #bindings;
  #name;
  // The parent logger's context, for child loggers.
  #parent;
  // ConsoleProvider's SerializedBindings for #bindings, once computed.
  #serializedBindings;

  constructor(bindings, name, parent) {
    this.bindings = bindings;
    this.name = name;
    this.#bindings = bindings;
    this.#name = name;
    this.#parent = parent;
  }

  static {
    getContextSerializedBindings = (context, bindings) => {
      if (typeof context !== 'object' || context === null ||
          !(#bindings in context) || context.#bindings !== bindings) {
        return undefined;
      }
      let serialized = context.#serializedBindings;
      if (serialized === undefined) {
        // The bindings are frozen, and only cached if all values are
        // primitives (see serializeFields()), so this cannot go stale.
        const parent = context.#parent;
        const name = context.#name;
        if (parent !== undefined) {
          serialized = composeBindings(
            bindings,
            parent.#bindings,
            getContextSerializedBindings(parent, parent.#bindings),
            name);
        }
        serialized ??= serializeBindings(bindings, name);
        context.#serializedBindings = serialized;
      }
      return serialized;
    };
  }
}
// Contexts look like plain null-prototype objects to providers.
ObjectSetPrototypeOf(LoggerContext.prototype, null);
delete LoggerContext.prototype.constructor;
ObjectFreeze(LoggerContext.prototype);

// Returns whether `obj` is an event created by Logger. Set in LogEvent's
// static block.
let isLogEvent;

class LogEvent {
  // Lets ConsoleProvider recognize events created by Logger. Cheaper than
  // checking the prototype.
  #brand;

  constructor(attributes, bindings, level, message, name, timestamp) {
    this.attributes = attributes;
    this.bindings = bindings;
    this.level = level;
    this.message = message;
    this.name = name;
    this.timestamp = timestamp;
  }

  static {
    isLogEvent = (obj) => #brand in obj;
  }
}
// Events look like plain null-prototype objects to providers.
ObjectSetPrototypeOf(LogEvent.prototype, null);
delete LogEvent.prototype.constructor;
ObjectFreeze(LogEvent.prototype);

const kEmpty = ObjectFreeze(new NullPrototypeObject());
const kDestinations = ['stdout', 'stderr'];
const kEmptyAttributes = kEmpty;
const kEmptyBindings = kEmpty;

// Objects created per event (events, attribute copies, custom levels passed
// to log(), records) are not frozen: each is new, so mutating one affects
// nothing else, consistent with EventEmitter and diagnostics_channel.
// Objects that are shared or reused (the built-in levels, provider level
// thresholds, logger bindings, kEmpty, kNullPrototype) are frozen.

// The built-in levels are shared by every logger and provider.
let levels = {
  trace: ObjectFreeze(new Level('trace', 10)),
  debug: ObjectFreeze(new Level('debug', 20)),
  info: ObjectFreeze(new Level('info', 30)),
  warn: ObjectFreeze(new Level('warn', 40)),
  error: ObjectFreeze(new Level('error', 50)),
  fatal: ObjectFreeze(new Level('fatal', 60)),
};
ObjectSetPrototypeOf(levels, null);
levels = ObjectFreeze(levels);
const kLevelNames = ObjectKeys(levels);

function normalizeLevel(level, name = 'level') {
  if (typeof level === 'string') {
    validateOneOf(level, name, kLevelNames);
    return levels[level];
  }

  validateObject(level, name);
  const { name: levelName, value } = level;
  validateString(levelName, `${name}.name`);
  validateInteger(value, `${name}.value`);
  return new Level(levelName, value);
}

// Provider level thresholds are frozen: they are exposed through the
// provider's `level` getter, and mutating one would change which levels are
// enabled without re-resolving loggers' level methods.
function normalizeThreshold(level, name = 'level') {
  return ObjectFreeze(normalizeLevel(level, name));
}

// Copies the own enumerable properties of `value`, like ObjectAssign().
// With `resolveFunctions`, function values are replaced by their return
// value, in the same pass.
function copyObject(value, resolveFunctions = false) {
  if (!resolveFunctions) {
    return ObjectAssign(new NullPrototypeObject(), value);
  }
  const copy = new NullPrototypeObject();
  const keys = ObjectKeys(value);
  for (let i = 0; i < keys.length; i++) {
    copyResolvedProperty(copy, value, keys[i]);
  }
  const symbols = ObjectGetOwnPropertySymbols(value);
  for (let i = 0; i < symbols.length; i++) {
    if (ObjectPrototypePropertyIsEnumerable(value, symbols[i])) {
      copyResolvedProperty(copy, value, symbols[i]);
    }
  }
  return copy;
}

function copyResolvedProperty(copy, value, key) {
  const entry = value[key];
  copy[key] = typeof entry === 'function' ?
    FunctionPrototypeCall(entry) : entry;
}

// Returns whether any own enumerable property (the ones copyObject() and
// ObjectAssign() copy) is a function. ObjectKeys() and
// ObjectGetOwnPropertySymbols() are much cheaper than ReflectOwnKeys().
function hasFunction(value) {
  const keys = ObjectKeys(value);
  for (let i = 0; i < keys.length; i++) {
    if (typeof value[keys[i]] === 'function') {
      return true;
    }
  }
  const symbols = ObjectGetOwnPropertySymbols(value);
  for (let i = 0; i < symbols.length; i++) {
    if (ObjectPrototypePropertyIsEnumerable(value, symbols[i]) &&
        typeof value[symbols[i]] === 'function') {
      return true;
    }
  }
  return false;
}

function isProvider(value) {
  return value !== null &&
    (typeof value === 'object' || typeof value === 'function') &&
    typeof value.log === 'function';
}

function validateProvider(provider, name = 'provider') {
  if (provider === null ||
      (typeof provider !== 'object' && typeof provider !== 'function')) {
    throw new ERR_INVALID_ARG_TYPE(name, 'Provider', provider);
  }

  validateFunction(provider.log, `${name}.log`);
  if (provider.isEnabled !== undefined) {
    validateFunction(provider.isEnabled, `${name}.isEnabled`);
  }
}

function isProviderEnabled(provider, level, context) {
  const isEnabled = provider.isEnabled;
  return isEnabled === undefined || FunctionPrototypeCall(
    isEnabled,
    provider,
    level,
    context,
  ) !== false;
}

// Level method dispatch
//
// When a provider's enablement can only change through paths this module
// observes, the per-level methods (`trace()`, `debug()`, etc.) are resolved
// ahead of time instead of checking `isEnabled()` on every call. Disabled
// levels are bound to a shared empty function, which the JIT can inline away
// entirely at monomorphic call sites (the same technique pino uses).
//
// The resolved methods live on a dispatch prototype that is shared by every
// logger with the same provider and the same logger prototype. Re-resolving
// after a level change therefore updates a single object, rather than having
// to track every logger instance (child loggers are often created per
// request).
//
// Providers whose enablement may change without notice (a custom
// `isEnabled()`, or `DiagnosticsProvider`, which depends on channel
// subscriptions) keep the per-call `isEnabled()` check.

function noop() {}

// Populated after the Logger class is defined: level name -> the real
// (checking) Logger.prototype method.
let kLevelMethods;
// Populated after the Logger class is defined. See getDispatchPrototype().
let createDispatchClass;
let isAggregateDispatchable;
let aggregateIsEnabled;

// Built-in isEnabled() implementations whose result only changes through a
// level setter that calls invalidateDispatch(). Populated by the providers.
const kObservableIsEnabled = new SafeSet();

// provider -> { prototypes: SafeMap<base, dispatch>, dependents: SafeSet }
const dispatchStates = new SafeWeakMap();
// Maps dispatch prototype -> { provider, base, target }, where `target` is
// the generated Logger subclass whose prototype is the dispatch object, or
// undefined for dispatch objects of other bases.
const dispatchInfo = new SafeWeakMap();

function getDispatchState(provider) {
  let state = dispatchStates.get(provider);
  if (state === undefined) {
    state = {
      prototypes: new SafeMap(),
      // WeakRefs to aggregate providers that must be re-resolved when this
      // provider's enablement changes.
      dependents: new SafeSet(),
    };
    dispatchStates.set(provider, state);
  }
  return state;
}

/**
 * Returns whether every change to the provider's enablement is observed by
 * this module, making it safe to resolve level methods ahead of time.
 * @param {object} provider
 * @returns {boolean}
 */
function isDispatchable(provider) {
  const isEnabled = provider.isEnabled;
  return isEnabled === undefined ||
    kObservableIsEnabled.has(isEnabled) ||
    (isEnabled === aggregateIsEnabled && isAggregateDispatchable(provider));
}

function resolveDispatchPrototype(provider, dispatch) {
  for (let i = 0; i < kLevelNames.length; i++) {
    const name = kLevelNames[i];
    // Level methods overridden by a Logger subclass are not managed.
    if (!ObjectPrototypeHasOwnProperty(dispatch, name)) continue;
    // Built-in dispatchable providers do not depend on the logger context.
    const method = isProviderEnabled(provider, levels[name], undefined) ?
      kLevelMethods[name] : noop;
    // Avoid needless writes, which invalidate optimized code.
    if (dispatch[name] !== method) dispatch[name] = method;
  }
}

/**
 * Returns the dispatch prototype for loggers using `provider` whose prototype
 * would otherwise be `base`, or `undefined` if level methods must check the
 * provider on every call.
 * @param {object} provider
 * @param {object} base
 * @returns {object|undefined}
 */
function getDispatchPrototype(provider, base) {
  // Without isEnabled() every level is always enabled, so the regular
  // prototype methods are already the right ones.
  if (provider.isEnabled === undefined || !isDispatchable(provider)) {
    return undefined;
  }
  const state = getDispatchState(provider);
  let dispatch = state.prototypes.get(base);
  if (dispatch === undefined) {
    // For plain loggers, the dispatch prototype is the prototype of a
    // generated Logger subclass, so that create() and child() can allocate
    // loggers with it directly using `new`. (Changing the prototype of an
    // existing object, and ReflectConstruct() with another new.target, are
    // both much slower.) Other bases only get the prototype swapped in.
    const target = createDispatchClass(base);
    dispatch = target === undefined ? { __proto__: base } : target.prototype;
    for (let i = 0; i < kLevelNames.length; i++) {
      const name = kLevelNames[i];
      if (base[name] !== kLevelMethods[name]) continue;
      const desc = {
        configurable: true,
        enumerable: false,
        value: noop,
        writable: true,
      };
      ObjectSetPrototypeOf(desc, null);
      ObjectDefineProperty(dispatch, name, desc);
    }
    resolveDispatchPrototype(provider, dispatch);
    state.prototypes.set(base, dispatch);
    dispatchInfo.set(dispatch, {
      __proto__: null,
      base,
      provider,
      target,
    });
  }
  return dispatch;
}


/**
 * Re-resolves level methods for all loggers using `provider`, and for all
 * aggregate providers that include it. Must be called whenever the result of
 * a dispatchable provider's isEnabled() may have changed.
 * @param {object} provider
 */
function invalidateDispatch(provider) {
  const state = dispatchStates.get(provider);
  if (state === undefined) return;
  for (const dispatch of state.prototypes.values()) {
    resolveDispatchPrototype(provider, dispatch);
  }
  for (const ref of state.dependents) {
    const dependent = ref.deref();
    if (dependent === undefined) {
      state.dependents.delete(ref);
    } else {
      invalidateDispatch(dependent);
    }
  }
}

function serializeError(error) {
  const {
    message,
    name,
    stack,
    code,
    cause,
    errors,
  } = error;
  const detail = { message, name, stack };
  const withCode = code !== undefined ? { code } : null;
  const withCause = cause !== undefined ? { cause } : null;
  const withErrors = errors !== undefined ? { errors } : null;
  return ObjectAssign(new NullPrototypeObject(), error, detail,
                      withCode, withCause, withErrors);
}

function defaultSerializer(event) {
  const ancestors = [];
  // Maps serialized errors back to the original. Only created if needed.
  let replacements;

  return JSONStringify(event, function replacer(_key, value) {
    if (typeof value === 'bigint') return String(value);
    if (value === null || typeof value !== 'object') return value;

    const holder = replacements?.get(this) ?? this;
    while (ancestors.length > 0 &&
           ancestors[ancestors.length - 1] !== holder) {
      ArrayPrototypePop(ancestors);
    }
    if (ArrayPrototypeIncludes(ancestors, value)) return '[Circular]';
    ArrayPrototypePush(ancestors, value);

    if (isError(value)) {
      const serialized = serializeError(value);
      replacements ??= new SafeWeakMap();
      replacements.set(serialized, value);
      return serialized;
    }
    return value;
  });
}

// Fast path for ConsoleProvider with the default serializer
//
// Builds the JSON text for a Logger event directly, without creating a
// record object or calling JSON.stringify() with a replacer for every value.
// It produces exactly the output of defaultSerializer() for the record, and
// gives up (returns undefined) whenever that might not hold: any value that
// is not a primitive or an Error (objects, arrays, BigInt), errors that
// define toJSON() or `errors` or are nested too deeply, and, when
// flattening, any key that collides with another or is an array index
// (which JSON orders first).

// Returned when the generic serializer must be used.
const kGeneric = Symbol('kGeneric');

// Errors nested deeper than this (through `cause` or other properties) use
// the generic serializer. This also stops cycles, which it reports as
// '[Circular]'.
const kMaxErrorDepth = 8;

// Short strings without characters that JSON escapes are quoted directly.
// Surrogates are left to JSONStringify(), which escapes lone ones.
function serializeString(value) {
  if (value.length > 64) return JSONStringify(value);
  for (let i = 0; i < value.length; i++) {
    const code = StringPrototypeCharCodeAt(value, i);
    if (code < 0x20 || code === 0x22 || code === 0x5c ||
        (code >= 0xd800 && code <= 0xdfff)) {
      return JSONStringify(value);
    }
  }
  return `"${value}"`;
}

// Property name -> `"name":`. Bounded, since keys may be data.
const kMaxCachedKeys = 1024;
const serializedKeyCache = new SafeMap();

function serializeKey(key) {
  let json = serializedKeyCache.get(key);
  if (json === undefined) {
    json = `${serializeString(key)}:`;
    if (serializedKeyCache.size < kMaxCachedKeys) {
      serializedKeyCache.set(key, json);
    }
  }
  return json;
}

function isIndexLikeKey(key) {
  // JSON orders array index keys first. Conservatively treat any key
  // starting with a digit as one.
  const code = StringPrototypeCharCodeAt(key, 0);
  return code >= 48 && code <= 57;
}

// Returns the JSON text for a primitive value, undefined if JSON omits it,
// or kGeneric.
function serializePrimitive(value) {
  switch (typeof value) {
    case 'string': return serializeString(value);
    case 'number': return NumberIsFinite(value) ? `${value}` : 'null';
    case 'boolean': return value ? 'true' : 'false';
    case 'undefined':
    case 'function':
    case 'symbol':
      return undefined;
    case 'object':
      return value === null ? 'null' : kGeneric;
    default:
      // BigInt: may have a user-defined toJSON().
      return kGeneric;
  }
}

// Like serializePrimitive(), but also serializes errors.
function serializeValue(value, depth) {
  if (typeof value === 'object' && value !== null && isError(value)) {
    return serializeErrorFast(value, depth);
  }
  return serializePrimitive(value);
}

/**
 * Serializes an error the way defaultSerializer() does (see
 * serializeError()): its own enumerable properties, followed by `message`,
 * `name`, `stack`, `code`, and `cause` if not already present.
 * @param {Error} error
 * @param {number} depth
 * @returns {string|symbol} The JSON text, or kGeneric.
 */
function serializeErrorFast(error, depth) {
  if (depth >= kMaxErrorDepth || typeof error.toJSON === 'function') {
    return kGeneric;
  }
  const { message, name, stack, code, cause, errors } = error;
  if (errors !== undefined) return kGeneric;

  const keys = ObjectKeys(error);
  let out = '';
  let hasMessage = false;
  let hasName = false;
  let hasStack = false;
  let hasCode = false;
  let hasCause = false;
  for (let i = 0; i < keys.length; i++) {
    const key = keys[i];
    let value;
    switch (key) {
      case 'message': hasMessage = true; value = message; break;
      case 'name': hasName = true; value = name; break;
      case 'stack': hasStack = true; value = stack; break;
      case 'code': hasCode = true; value = code; break;
      case 'cause': hasCause = true; value = cause; break;
      case 'errors': continue; // Undefined, checked above.
      default:
        if (isIndexLikeKey(key)) return kGeneric;
        value = error[key];
    }
    const json = serializeValue(value, depth + 1);
    if (json === kGeneric) return kGeneric;
    if (json !== undefined) {
      out += `${out === '' ? '' : ','}${serializeKey(key)}${json}`;
    }
  }

  // Appended in this order, if not own enumerable properties of the error.
  if (!hasMessage) out = appendErrorField(out, 'message', message, depth);
  if (!hasName && out !== kGeneric) {
    out = appendErrorField(out, 'name', name, depth);
  }
  if (!hasStack && out !== kGeneric) {
    out = appendErrorField(out, 'stack', stack, depth);
  }
  if (!hasCode && code !== undefined && out !== kGeneric) {
    out = appendErrorField(out, 'code', code, depth);
  }
  if (!hasCause && cause !== undefined && out !== kGeneric) {
    out = appendErrorField(out, 'cause', cause, depth);
  }
  return out === kGeneric ? kGeneric : `{${out}}`;
}

function appendErrorField(out, key, value, depth) {
  const json = serializeValue(value, depth + 1);
  if (json === kGeneric) return kGeneric;
  if (json === undefined) return out;
  return `${out}${out === '' ? '' : ','}${serializeKey(key)}${json}`;
}

// Keys that a flattened record sets itself, or that change JSON key order.
function isFlatReservedKey(key) {
  switch (key) {
    case 'level':
    case 'levelName':
    case 'message':
    case 'name':
    case 'pid':
    case 'timestamp':
      return true;
    default:
      return isIndexLikeKey(key);
  }
}

/**
 * Serializes the own enumerable string-keyed properties of `obj` as
 * comma-separated `"key":value` pairs.
 * @param {object} obj
 * @param {boolean} allowErrors Whether values may be errors. Bindings may
 *   not, since their serialization is cached and errors are mutable.
 * @param {object} [flatBindings] When flattening attributes, the bindings
 *   they are merged with. Reserved and colliding keys return kGeneric.
 * @returns {string|symbol} The pairs, or kGeneric.
 */
function serializeFields(obj, allowErrors, flatBindings,
                         keys = ObjectKeys(obj)) {
  let out = '';
  for (let i = 0; i < keys.length; i++) {
    const key = keys[i];
    if (flatBindings !== undefined &&
        (isFlatReservedKey(key) ||
         ObjectPrototypeHasOwnProperty(flatBindings, key))) {
      return kGeneric;
    }
    const value = allowErrors ?
      serializeValue(obj[key], 0) : serializePrimitive(obj[key]);
    if (value === undefined) continue;
    if (value === kGeneric) return kGeneric;
    out += `${out === '' ? '' : ','}${serializeKey(key)}${value}`;
  }
  return out;
}

// Serialized bindings (and name) of a logger.
function SerializedBindings(fields, flatSafe, keys, name, nameFragment) {
  this.fields = fields;
  this.flatSafe = flatSafe;
  // ObjectKeys() of the bindings, for composeBindings().
  this.keys = keys;
  this.name = name;
  // `,"name":<name>`, or '' if there is no name.
  this.nameFragment = nameFragment ?? (name === undefined ? '' :
    `,"name":${serializeString(name)}`);
}
SerializedBindings.prototype = kNullPrototype;

// Serializes bindings for both nested and flattened records.
function serializeBindings(bindings, name) {
  const keys = ObjectKeys(bindings);
  const fields = serializeFields(bindings, false, undefined, keys);
  let flatSafe = fields !== kGeneric;
  if (flatSafe) {
    for (let i = 0; i < keys.length; i++) {
      if (isFlatReservedKey(keys[i])) {
        flatSafe = false;
        break;
      }
    }
  }
  return new SerializedBindings(fields, flatSafe, keys, name);
}

/**
 * Serializes a child logger's bindings by extending its parent's
 * serialization, if the child's bindings start with exactly the parent's
 * keys and values (no array index keys, no overridden values).
 * @param {object} bindings The child's merged bindings.
 * @param {object} parentBindings
 * @param {SerializedBindings} parentSerialized
 * @param {string|undefined} name The child's name.
 * @returns {SerializedBindings|undefined} undefined if not applicable.
 */
function composeBindings(bindings, parentBindings, parentSerialized, name) {
  if (parentSerialized.fields === kGeneric) return undefined;
  const keys = ObjectKeys(bindings);
  const parentKeys = parentSerialized.keys;
  const parentLength = parentKeys.length;
  if (keys.length < parentLength) return undefined;
  for (let i = 0; i < parentLength; i++) {
    const key = parentKeys[i];
    if (keys[i] !== key || bindings[key] !== parentBindings[key]) {
      return undefined;
    }
  }
  let fields = '';
  let flatSafe = parentSerialized.flatSafe;
  for (let i = parentLength; i < keys.length; i++) {
    const key = keys[i];
    if (isFlatReservedKey(key)) flatSafe = false;
    const value = serializePrimitive(bindings[key]);
    if (value === kGeneric) return undefined;
    if (value === undefined) continue;
    fields += `${fields === '' ? '' : ','}${serializeKey(key)}${value}`;
  }
  return new SerializedBindings(
    joinFields(parentSerialized.fields, fields),
    flatSafe,
    keys,
    name,
    name === parentSerialized.name ? parentSerialized.nameFragment : undefined);
}

// Serialized bindings are cached per logger (in its context) when the event
// carries the logger's own frozen bindings, i.e. no binding is a function.
function getSerializedBindings(bindings, name, context) {
  const serialized = getContextSerializedBindings(context, bindings);
  if (serialized !== undefined && serialized.name === name) return serialized;
  return serializeBindings(bindings, name);
}

// The serialized timestamp of the previous event, reused while consecutive
// events have the same timestamp.
let lastTimestamp = -1;
let lastTimestampJSON = '';

function SerializedLevel(nested, flat) {
  // `"level":{"name":...,"value":...}` contents and flattened fields.
  this.nested = nested;
  this.flat = flat;
}
SerializedLevel.prototype = kNullPrototype;

function serializeLevel(name, value) {
  const nameJSON = serializeString(name);
  return new SerializedLevel(`{"name":${nameJSON},"value":${value}}`,
                             `"level":${value},"levelName":${nameJSON}`);
}

// Built-in level -> SerializedLevel.
const serializedLevels = new SafeMap();
for (let i = 0; i < kLevelNames.length; i++) {
  const level = levels[kLevelNames[i]];
  serializedLevels.set(level, serializeLevel(level.name, level.value));
}

function joinFields(a, b) {
  if (a === '') return b;
  if (b === '') return a;
  return `${a},${b}`;
}

/**
 * Returns the line (what defaultSerializer() would produce for the record
 * ConsoleProvider creates from `event`, followed by a newline), or undefined
 * if the generic path must be used.
 * @param {object} event
 * @param {object} [context] The context passed to the provider.
 * @param {boolean} flatten
 * @param {string} pidJSON `,"pid":<pid>` if enabled, otherwise ''.
 * @returns {string|undefined}
 */
function serializeEventFast(event, context, flatten, pidJSON) {
  if (!isLogEvent(event)) return undefined;
  const { attributes, bindings, level, message, name, timestamp } = event;
  if (typeof attributes !== 'object' || attributes === null ||
      typeof bindings !== 'object' || bindings === null ||
      typeof level !== 'object' || level === null ||
      typeof timestamp !== 'number') {
    return undefined;
  }
  let serializedLevel = serializedLevels.get(level);
  if (serializedLevel === undefined) {
    const levelName = level.name;
    const levelValue = level.value;
    if (typeof levelName !== 'string' || !NumberIsFinite(levelValue)) {
      return undefined;
    }
    serializedLevel = serializeLevel(levelName, levelValue);
  }
  const messageJSON = serializeValue(message, 0);
  if (messageJSON === kGeneric) return undefined;
  if (name !== undefined && typeof name !== 'string') return undefined;

  const serializedBindings = getSerializedBindings(bindings, name, context);
  if (serializedBindings.fields === kGeneric) return undefined;

  if (timestamp !== lastTimestamp) {
    lastTimestamp = timestamp;
    lastTimestampJSON = `${timestamp}`;
  }
  const tail =
    `${messageJSON === undefined ? '' : `,"message":${messageJSON}`}` +
    `${serializedBindings.nameFragment},"timestamp":${lastTimestampJSON}` +
    `${pidJSON}}\n`;

  if (flatten) {
    if (!serializedBindings.flatSafe) return undefined;
    const attributeFields = serializeFields(attributes, true, bindings);
    if (attributeFields === kGeneric) return undefined;
    const fields = joinFields(serializedBindings.fields, attributeFields);
    return `{${fields === '' ? '' : `${fields},`}${serializedLevel.flat}` +
      tail;
  }

  const attributeFields = serializeFields(attributes, true);
  if (attributeFields === kGeneric) return undefined;
  return `{"attributes":{${attributeFields}},` +
    `"bindings":{${serializedBindings.fields}},` +
    `"level":${serializedLevel.nested}${tail}`;
}

/**
 * A provider that fans log events out to multiple providers.
 */
class AggregateProvider {
  #dispatchable;
  #providers;

  static {
    aggregateIsEnabled = this.prototype.isEnabled;
    isAggregateDispatchable = (provider) => {
      return #dispatchable in provider && provider.#dispatchable;
    };
  }

  constructor(providers) {
    validateArray(providers, 'providers');
    const copy = ArrayPrototypeSlice(providers);
    let dispatchable = true;
    for (let i = 0; i < copy.length; i++) {
      validateProvider(copy[i], `providers[${i}]`);
      dispatchable &&= isDispatchable(copy[i]);
    }
    this.#providers = ObjectFreeze(copy);
    this.#dispatchable = dispatchable;

    if (dispatchable) {
      // Re-resolve loggers using this aggregate whenever a member's
      // enablement changes. Members without isEnabled() never change.
      let ref;
      for (let i = 0; i < copy.length; i++) {
        const provider = copy[i];
        if (provider.isEnabled === undefined) continue;
        ref ??= new SafeWeakRef(this);
        // Duplicate members share the ref, so the SafeSet dedupes them.
        getDispatchState(provider).dependents.add(ref);
      }
    }
  }

  get providers() {
    return this.#providers;
  }

  isEnabled(level, context) {
    for (let i = 0; i < this.#providers.length; i++) {
      if (isProviderEnabled(this.#providers[i], level, context)) return true;
    }
    return false;
  }

  log(event, context) {
    for (let i = 0; i < this.#providers.length; i++) {
      const provider = this.#providers[i];
      if (isProviderEnabled(provider, event.level, context)) {
        FunctionPrototypeCall(provider.log, provider, event, context);
      }
    }
  }
}

/**
 * A provider that writes newline-delimited JSON to stdout or stderr.
 */
class ConsoleProvider {
  #destination;
  #flatten;
  #level;
  #pid;
  #pidJSON;
  #serializer;
  #stream;

  static {
    kObservableIsEnabled.add(this.prototype.isEnabled);
  }

  constructor(options = kEmptyObject) {
    validateObject(options, 'options');
    const {
      destination = 'stdout',
      flatten = false,
      level = 'info',
      maxLength,
      minLength,
      periodicFlush,
      pid = false,
      serializer = defaultSerializer,
      sync,
    } = options;

    validateOneOf(destination, 'options.destination', kDestinations);
    validateBoolean(flatten, 'options.flatten');
    validateBoolean(pid, 'options.pid');
    validateFunction(serializer, 'options.serializer');
    this.#destination = destination;
    this.#flatten = flatten;
    this.#level = normalizeThreshold(level, 'options.level');
    this.#pid = pid;
    this.#pidJSON = pid ? `,"pid":${process.pid}` : '';
    this.#serializer = serializer;
    this.#stream = new Utf8Stream({
      // Creating the logger is not a hotpath... it's ok for the options
      // to be passed in dictionary mode rather than fast properties mode.
      __proto__: null,
      fd: destination === 'stdout' ? 1 : 2,
      maxLength,
      minLength,
      periodicFlush,
      sync,
    });
  }

  get destination() {
    return this.#destination;
  }

  get flatten() {
    return this.#flatten;
  }

  get level() {
    return this.#level;
  }

  set level(level) {
    this.#level = normalizeThreshold(level);
    invalidateDispatch(this);
  }

  get pid() {
    return this.#pid;
  }

  get serializer() {
    return this.#serializer;
  }

  get stream() {
    return this.#stream;
  }

  isEnabled(level) {
    return level.value >= this.#level.value;
  }

  log(event, context) {
    if (this.isEnabled(event.level)) {
      if (this.#serializer === defaultSerializer) {
        const serialized =
          serializeEventFast(event, context, this.#flatten, this.#pidJSON);
        if (serialized !== undefined) {
          this.#stream.write(serialized);
          return;
        }
      }
      let record = event;
      if (this.#flatten || this.#pid) {
        const withPid = this.#pid ? { pid: process.pid } : null;
        record = new NullPrototypeObject();
        if (this.#flatten) {
          record = ObjectAssign(
            record,
            event.bindings,
            event.attributes,
            {
              level: event.level.value,
              levelName: event.level.name,
              message: event.message,
              name: event.name,
              timestamp: event.timestamp,
            },
            withPid,
          );
        } else {
          record = ObjectAssign(record, event, withPid);
        }
      }
      const serialized = FunctionPrototypeCall(
        this.#serializer,
        undefined,
        record,
      );
      if (typeof serialized !== 'string') {
        throw new ERR_INVALID_RETURN_VALUE(
          'a string',
          'serializer',
          serialized,
        );
      }
      this.#stream.write(`${serialized}\n`);
    }
  }

  flush(callback) {
    this.#stream.flush(callback);
  }

  flushSync() {
    this.#stream.flushSync();
  }
}

/**
 * A provider that publishes log events to diagnostics channels.
 */
class DiagnosticsProvider {
  #router;
  #routes;

  constructor(channels) {
    if (typeof channels === 'function') {
      this.#router = channels;
      return;
    }

    validateObject(channels, 'channels');
    const names = ReflectOwnKeys(channels);
    const routes = [];
    for (let i = 0; i < names.length; i++) {
      const name = names[i];
      const selector = channels[name];
      validateFunction(selector, `channels[${i}]`);
      const obj = {
        channel: diagnosticsChannel(name),
        selector,
      };
      ObjectSetPrototypeOf(obj, null);
      ArrayPrototypePush(routes, obj);
    }
    this.#routes = routes;
  }

  isEnabled(level) {
    if (this.#router !== undefined) {
      const channel = this.#getChannel(level);
      return channel?.hasSubscribers === true;
    }

    for (let i = 0; i < this.#routes.length; i++) {
      const route = this.#routes[i];
      if (route.channel.hasSubscribers &&
          FunctionPrototypeCall(route.selector, undefined, level)) {
        return true;
      }
    }
    return false;
  }

  log(event) {
    if (this.#router !== undefined) {
      const channel = this.#getChannel(event.level);
      if (channel?.hasSubscribers) channel.publish(event);
      return;
    }

    for (let i = 0; i < this.#routes.length; i++) {
      const route = this.#routes[i];
      if (route.channel.hasSubscribers &&
          FunctionPrototypeCall(route.selector, undefined, event.level)) {
        route.channel.publish(event);
      }
    }
  }

  #getChannel(level) {
    const name = FunctionPrototypeCall(this.#router, undefined, level);
    return name === undefined ? undefined : diagnosticsChannel(name);
  }
}

/**
 * An EventEmitter that receives structured log events.
 */
class EventProvider extends EventEmitter {
  #level;

  static {
    kObservableIsEnabled.add(this.prototype.isEnabled);
  }

  constructor(options = kEmptyObject) {
    validateObject(options, 'options');
    const { level = 'trace' } = options;
    const normalizedLevel = normalizeThreshold(level, 'options.level');

    super();
    this.#level = normalizedLevel;
  }

  get level() {
    return this.#level;
  }

  set level(level) {
    this.#level = normalizeThreshold(level);
    invalidateDispatch(this);
  }

  isEnabled(level) {
    return level.value >= this.#level.value;
  }

  log(event) {
    if (this.isEnabled(event.level)) {
      const eventName = event.level.name;
      this.emit(this.listenerCount(eventName) > 0 ? eventName : 'log', event);
    }
  }
}

let isLogger;

// Passed as the first constructor argument by child(), which has already
// validated its arguments, copied the bindings, and allocates the child with
// the right prototype. Not reachable by users.
const kLoggerInit = Symbol('kLoggerInit');

function LoggerInit(provider, name, bindings, childTarget, parentContext) {
  this.provider = provider;
  this.name = name;
  this.bindings = bindings;
  this.childTarget = childTarget;
  this.parentContext = parentContext;
}
LoggerInit.prototype = kNullPrototype;

class Logger {
  #bindings;
  // The class child() uses to allocate children with the provider's dispatch
  // prototype: undefined until first needed, null if there is none.
  #childTarget;
  #context;
  // Whether any binding is a function, or undefined until #materialize().
  #hasFunction;
  #name;
  #provider;

  static {
    isLogger = (obj) => {
      return obj !== null && typeof obj === 'object' && #provider in obj;
    };
  }

  constructor(providerOrOptions, options) {
    if (providerOrOptions === kLoggerInit) {
      // Internal (child()): already validated, bindings already copied, and
      // allocated with the right prototype. Freezing the bindings and looking
      // for functions in them is deferred (see #materialize()).
      this.#init(options.provider, options.name, options.bindings,
                 undefined, options.parentContext);
      this.#childTarget = options.childTarget;
      return;
    }

    emitExperimentalWarning('Logger');

    let provider = providerOrOptions;
    if (provider == null) {
      provider = getDefaultProvider();
    } else if (options === undefined &&
               typeof provider === 'object' &&
               !isProvider(provider)) {
      options = provider;
      provider = getDefaultProvider();
    }

    options ??= kEmptyObject;
    validateProvider(provider);
    validateObject(options, 'options');

    const {
      bindings = kEmptyBindings,
      name,
    } = options;
    validateObject(bindings, 'options.bindings');
    if (name !== undefined) {
      validateString(name, 'options.name');
    }

    // Frozen, since the same object is passed as event.bindings for every
    // event when no binding is a function.
    const copy = ObjectFreeze(copyObject(bindings));
    this.#init(provider, name, copy, hasFunction(copy));

    // Interpose the shared dispatch prototype so that methods for disabled
    // levels resolve to an empty function. create() and child() allocate
    // loggers with it already in place (see constructLogger()). Otherwise,
    // e.g. for `new Logger()` and subclasses, it is swapped in here, using
    // the current prototype as the base so that subclasses keep their own
    // methods.
    const proto = ObjectGetPrototypeOf(this);
    const info = dispatchInfo.get(proto);
    if (info?.provider !== provider) {
      // A dispatch prototype belonging to another provider (only reachable
      // via a crafted new.target) is replaced, starting from its base.
      const base = info === undefined ? proto : info.base;
      const dispatch = getDispatchPrototype(provider, base) ?? base;
      if (dispatch !== proto) {
        ObjectSetPrototypeOf(this, dispatch);
      }
    }
  }

  #init(provider, name, bindings, hasFunction, parentContext) {
    this.#provider = provider;
    this.#name = name;
    this.#bindings = bindings;
    this.#hasFunction = hasFunction;
    this.#context = new LoggerContext(bindings, name, parentContext);
  }

  get provider() {
    if (!isLogger(this)) {
      throw new ERR_INVALID_THIS('Logger');
    }
    return this.#provider;
  }

  get name() {
    if (!isLogger(this)) {
      throw new ERR_INVALID_THIS('Logger');
    }
    return this.#name;
  }

  get bindings() {
    if (!isLogger(this)) {
      throw new ERR_INVALID_THIS('Logger');
    }
    if (this.#hasFunction === undefined) this.#materialize();
    return this.#bindings;
  }

  isEnabled(level) {
    if (!isLogger(this)) {
      throw new ERR_INVALID_THIS('Logger');
    }
    return this.#isEnabled(normalizeLevel(level));
  }

  log(level, message, attributes = kEmptyAttributes) {
    if (!isLogger(this)) {
      throw new ERR_INVALID_THIS('Logger');
    }
    this.#log(normalizeLevel(level), message, attributes);
  }

  trace(message, attributes = kEmptyAttributes) {
    if (!isLogger(this)) {
      throw new ERR_INVALID_THIS('Logger');
    }
    this.#log(levels.trace, message, attributes);
  }

  debug(message, attributes = kEmptyAttributes) {
    if (!isLogger(this)) {
      throw new ERR_INVALID_THIS('Logger');
    }
    this.#log(levels.debug, message, attributes);
  }

  info(message, attributes = kEmptyAttributes) {
    if (!isLogger(this)) {
      throw new ERR_INVALID_THIS('Logger');
    }
    this.#log(levels.info, message, attributes);
  }

  warn(message, attributes = kEmptyAttributes) {
    if (!isLogger(this)) {
      throw new ERR_INVALID_THIS('Logger');
    }
    this.#log(levels.warn, message, attributes);
  }

  error(message, attributes = kEmptyAttributes) {
    if (!isLogger(this)) {
      throw new ERR_INVALID_THIS('Logger');
    }
    this.#log(levels.error, message, attributes);
  }

  fatal(message, attributes = kEmptyAttributes) {
    if (!isLogger(this)) {
      throw new ERR_INVALID_THIS('Logger');
    }
    this.#log(levels.fatal, message, attributes);
  }

  child(bindings, options = kEmptyObject) {
    if (!isLogger(this)) {
      throw new ERR_INVALID_THIS('Logger');
    }
    validateObject(bindings, 'bindings');
    validateObject(options, 'options');

    const { name = this.#name } = options;
    if (name !== undefined) {
      validateString(name, 'options.name');
    }

    // Children share the provider, so they also share the class that
    // allocates them with its dispatch prototype. Resolved once per parent
    // and passed down to children.
    let childTarget = this.#childTarget;
    if (childTarget === undefined) {
      const dispatch = getDispatchPrototype(this.#provider, Logger.prototype);
      childTarget = dispatch === undefined ?
        null : dispatchInfo.get(dispatch).target;
      this.#childTarget = childTarget;
    }

    // Not frozen yet: many children never log (e.g. only disabled levels),
    // so that is deferred until the bindings are observable. Nothing else can
    // reach them before then, so they cannot change meanwhile.
    const init = new LoggerInit(
      this.#provider,
      name,
      ObjectAssign(new NullPrototypeObject(), this.#bindings, bindings),
      childTarget,
      this.#context,
    );
    return childTarget === null ?
      new Logger(kLoggerInit, init) :
      new childTarget(kLoggerInit, init);
  }

  // Completes the setup that child() defers. Called before the bindings
  // become observable: through the bindings getter, or through the context
  // passed to the provider (every call to the provider goes through
  // #isEnabled()).
  #materialize() {
    ObjectFreeze(this.#bindings);
    this.#hasFunction = hasFunction(this.#bindings);
  }

  #isEnabled(level) {
    if (this.#hasFunction === undefined) this.#materialize();
    return isProviderEnabled(this.#provider, level, this.#context);
  }

  #log(level, message, attributes) {
    if (!this.#isEnabled(level)) {
      return;
    }

    validateObject(attributes, 'attributes');

    const event = new LogEvent(
      attributes === kEmptyAttributes ?
        attributes : copyObject(attributes, true),
      this.#hasFunction ? copyObject(this.#bindings, true) : this.#bindings,
      level,
      message,
      this.#name,
      DateNow(),
    );

    FunctionPrototypeCall(
      this.#provider.log,
      this.#provider,
      event,
      this.#context,
    );
  }
}

createDispatchClass = (base) => {
  if (base !== Logger.prototype) return undefined;
  // Uses the default derived constructor, which V8 skips entirely.
  const DispatchLogger = class extends Logger {};
  ObjectDefineProperty(DispatchLogger.prototype, 'constructor', {
    __proto__: null,
    configurable: true,
    enumerable: false,
    value: Logger,
    writable: true,
  });
  return DispatchLogger;
};

kLevelMethods = {};
ObjectSetPrototypeOf(kLevelMethods, null);
for (let i = 0; i < kLevelNames.length; i++) {
  const name = kLevelNames[i];
  kLevelMethods[name] = Logger.prototype[name];
}

/**
 * Creates a Logger whose prototype is the provider's dispatch prototype from
 * the start, if it has one. Equivalent to `new Logger(...)`, which instead
 * has to swap the prototype after allocation.
 * @param {any} providerOrOptions
 * @param {object} [options]
 * @returns {Logger}
 */
function constructLogger(providerOrOptions, options) {
  // Resolve the provider the same way the constructor does, without
  // validating. Anything the constructor would reject goes through the
  // regular path so that errors are reported identically.
  let provider = providerOrOptions;
  if (provider == null ||
      (options === undefined && typeof provider === 'object' &&
       !isProvider(provider))) {
    provider = getDefaultProvider();
  }
  if (isProvider(provider)) {
    const dispatch = getDispatchPrototype(provider, Logger.prototype);
    if (dispatch !== undefined) {
      const DispatchLogger = dispatchInfo.get(dispatch).target;
      return new DispatchLogger(providerOrOptions, options);
    }
  }
  return new Logger(providerOrOptions, options);
}

function create(provider, options) {
  return constructLogger(provider, options);
}

let defaultProvider;

function getDefaultProvider() {
  defaultProvider ??= new ConsoleProvider();
  return defaultProvider;
}

let settingDefaultProvider = false;

function setDefaultProvider(provider) {
  if (settingDefaultProvider) {
    throw new ERR_INVALID_STATE('Already setting the default provider');
  }
  settingDefaultProvider = true;
  try {
    // Note that we validate the provider after the settingDefaultProvider
    // check. That is to prevent re-entrant calls that might happen when
    // we try to access the properties of the provider that is given.
    validateProvider(provider, 'defaultProvider');
    if (provider !== defaultProvider &&
      process.listenerCount('defaultLoggerProviderChanged')) {
      process.emit('defaultLoggerProviderChanged', provider, defaultProvider);
    }
    defaultProvider = provider;
  } finally {
    settingDefaultProvider = false;
  }
}

function getDescriptor(value) {
  const desc = {
    value,
    enumerable: true,
    writable: true,
    configurable: true,
  };
  ObjectSetPrototypeOf(desc, null);
  return desc;
}

const properties = {
  AggregateProvider: getDescriptor(AggregateProvider),
  ConsoleProvider: getDescriptor(ConsoleProvider),
  DiagnosticsProvider: getDescriptor(DiagnosticsProvider),
  EventProvider: getDescriptor(EventProvider),
  Logger: getDescriptor(Logger),
  create: getDescriptor(create),
  getDefaultProvider: getDescriptor(getDefaultProvider),
  levels: getDescriptor(levels),
  setDefaultProvider: getDescriptor(setDefaultProvider),
};
ObjectSetPrototypeOf(properties, null);
ObjectDefineProperties(create, properties);

module.exports = create;
