'use strict';

const {
  ArrayFrom,
  ArrayIsArray,
  ArrayPrototypePush,
  MapPrototypeClear,
  MapPrototypeEntries,
  MapPrototypeSet,
  ObjectDefineProperty,
  ObjectGetOwnPropertyDescriptor,
  ObjectGetOwnPropertyNames,
  ObjectGetPrototypeOf,
  ObjectKeys,
  ObjectPrototype,
  ObjectPrototypeHasOwnProperty,
  ObjectSetPrototypeOf,
  PromiseReject,
  PromiseResolve,
  PromiseWithResolvers,
  SafeMap,
  SetPrototypeAdd,
  SetPrototypeClear,
  SetPrototypeValues,
  Symbol,
} = primordials;
const Readable = require('internal/streams/readable');
const { deserializeError, serializeError } = require('internal/error_serdes');
const {
  codes: {
    ERR_INVALID_STATE,
  },
} = require('internal/errors');
const { isError } = require('internal/util');
const { isMap, isSet } = require('internal/util/types');
const { structuredClone } = require('internal/worker/js_transferable');

const kEmitMessage = Symbol('kEmitMessage');

function repairError(source, clone, seen) {
  const serialized = deserializeError(serializeError(source));
  let sourceName;
  try {
    sourceName = source.name;
  } catch {
    // The serialized form already omits properties whose getters throw.
  }
  let repaired = clone;
  if (!isError(repaired) ||
      (sourceName !== undefined && repaired.name !== sourceName)) {
    repaired = serialized;
  }
  if (repaired === null || typeof repaired !== 'object') return repaired;
  seen.set(source, repaired);

  if (serialized !== null && typeof serialized === 'object') {
    const serializedKeys = ObjectGetOwnPropertyNames(serialized);
    for (let i = 0; i < serializedKeys.length; i++) {
      const key = serializedKeys[i];
      if (ObjectPrototypeHasOwnProperty(repaired, key)) continue;
      const descriptor = ObjectGetOwnPropertyDescriptor(serialized, key);
      ObjectSetPrototypeOf(descriptor, null);
      ObjectDefineProperty(repaired, key, descriptor);
    }
  }

  const keys = ObjectGetOwnPropertyNames(source);
  for (let i = 0; i < keys.length; i++) {
    const key = keys[i];
    const descriptor = ObjectGetOwnPropertyDescriptor(source, key);
    if (!ObjectPrototypeHasOwnProperty(descriptor, 'value') ||
        typeof descriptor.value === 'function' ||
        typeof descriptor.value === 'symbol') {
      continue;
    }
    const existing = ObjectGetOwnPropertyDescriptor(repaired, key);
    const value = descriptor.value !== null &&
      typeof descriptor.value === 'object' ?
      repairClone(descriptor.value, existing?.value, seen) : descriptor.value;
    if ((existing !== undefined && existing.value === value) ||
        existing?.configurable === false) {
      continue;
    }
    ObjectDefineProperty(repaired, key, {
      __proto__: null,
      configurable: descriptor.configurable,
      enumerable: descriptor.enumerable,
      value,
      writable: descriptor.writable,
    });
  }
  return repaired;
}

function repairClone(source, clone, seen) {
  if (source === null || (typeof source !== 'object' &&
                          typeof source !== 'function')) {
    return clone === undefined ? source : clone;
  }
  if (typeof source === 'function') return clone;
  if (seen.has(source)) return seen.get(source);
  if (isError(source)) return repairError(source, clone, seen);
  if (clone === null || typeof clone !== 'object') {
    try {
      clone = structuredClone(source);
    } catch {
      const prototype = ObjectGetPrototypeOf(source);
      if (!ArrayIsArray(source) && prototype !== null &&
          prototype !== ObjectPrototype) {
        return clone;
      }
      clone = ArrayIsArray(source) ? [] : { __proto__: prototype };
    }
  }
  seen.set(source, clone);
  if (isMap(source) && isMap(clone)) {
    const sourceEntries = ArrayFrom(MapPrototypeEntries(source));
    const cloneEntries = ArrayFrom(MapPrototypeEntries(clone));
    const repairedEntries = [];
    for (let i = 0; i < sourceEntries.length; i++) {
      ArrayPrototypePush(repairedEntries, [
        repairClone(sourceEntries[i][0], cloneEntries[i][0], seen),
        repairClone(sourceEntries[i][1], cloneEntries[i][1], seen),
      ]);
    }
    MapPrototypeClear(clone);
    for (let i = 0; i < repairedEntries.length; i++) {
      MapPrototypeSet(clone, repairedEntries[i][0], repairedEntries[i][1]);
    }
    return clone;
  }
  if (isSet(source) && isSet(clone)) {
    const sourceValues = ArrayFrom(SetPrototypeValues(source));
    const cloneValues = ArrayFrom(SetPrototypeValues(clone));
    const repairedValues = [];
    for (let i = 0; i < sourceValues.length; i++) {
      ArrayPrototypePush(
        repairedValues, repairClone(sourceValues[i], cloneValues[i], seen));
    }
    SetPrototypeClear(clone);
    for (let i = 0; i < repairedValues.length; i++) {
      SetPrototypeAdd(clone, repairedValues[i]);
    }
    return clone;
  }
  const prototype = ObjectGetPrototypeOf(source);
  if (prototype === null) ObjectSetPrototypeOf(clone, null);
  if (prototype !== null && prototype !== ObjectPrototype &&
      !ArrayIsArray(source)) {
    return clone;
  }
  const keys = ObjectKeys(source);
  for (let i = 0; i < keys.length; i++) {
    const key = keys[i];
    const descriptor = ObjectGetOwnPropertyDescriptor(source, key);
    if (!ObjectPrototypeHasOwnProperty(descriptor, 'value')) continue;
    const existing = ObjectGetOwnPropertyDescriptor(clone, key);
    const value = repairClone(descriptor.value, existing?.value, seen);
    if ((existing !== undefined && existing.value === value) ||
        existing?.configurable === false) {
      continue;
    }
    ObjectDefineProperty(clone, key, {
      __proto__: null,
      configurable: descriptor.configurable,
      enumerable: descriptor.enumerable,
      value,
      writable: descriptor.writable,
    });
  }
  if (ArrayIsArray(source)) {
    const descriptor = ObjectGetOwnPropertyDescriptor(source, 'length');
    ObjectSetPrototypeOf(descriptor, null);
    ObjectDefineProperty(clone, 'length', descriptor);
  }
  return clone;
}

function cloneRecordData(data) {
  let clone;
  try {
    clone = structuredClone(data);
  } catch (error) {
    if (data.error === undefined) throw error;
    clone = structuredClone({ __proto__: null, ...data, error: undefined });
    clone.error = deserializeError(serializeError(data.error));
  }
  try {
    return repairClone(data, clone, new SafeMap());
  } catch {
    return clone;
  }
}

class BenchmarksStream extends Readable {
  #blocked = false;
  #drainWaiters = [];
  #hasReader = false;

  constructor() {
    super({
      __proto__: null,
      objectMode: true,
    });
  }

  _read() {
    if (this.#blocked) {
      this.#blocked = false;
      const waiters = this.#drainWaiters;
      this.#drainWaiters = [];
      for (let i = 0; i < waiters.length; i++) waiters[i].resolve();
    }
  }

  read(size) {
    if (size !== 0) this.#hasReader = true;
    return super.read(size);
  }

  _destroy(error, callback) {
    const failure = error ??
      new ERR_INVALID_STATE('benchmark stream is closed');
    const waiters = this.#drainWaiters;
    this.#drainWaiters = [];
    for (let i = 0; i < waiters.length; i++) waiters[i].reject(failure);
    callback(error);
  }

  waitForDrain() {
    if (this.destroyed) {
      return PromiseReject(this.errored ??
        new ERR_INVALID_STATE('benchmark stream is closed'));
    }
    if (!this.#blocked) return PromiseResolve();
    const waiter = PromiseWithResolvers();
    ArrayPrototypePush(this.#drainWaiters, waiter);
    return waiter.promise;
  }

  start(data) {
    return this[kEmitMessage]('bench:start', data);
  }

  sample(data) {
    return this[kEmitMessage]('bench:sample', data);
  }

  complete(data) {
    return this[kEmitMessage]('bench:complete', data);
  }

  diagnostic(data) {
    return this[kEmitMessage]('bench:diagnostic', data);
  }

  summary(data) {
    return this[kEmitMessage]('bench:summary', data);
  }

  end() {
    return this.#tryPush(null);
  }

  [kEmitMessage](type, data) {
    const recordData = cloneRecordData(data);
    const record = { __proto__: null, type, data: recordData };
    if (this.listenerCount(type) > 0) {
      this.emit(type, cloneRecordData(recordData));
    }
    return this.#tryPush(record);
  }

  #tryPush(record) {
    if (this.destroyed) return false;
    const canPush = this.push(record);
    if (record !== null && !canPush && this.#hasReader) {
      this.#blocked = true;
      return false;
    }
    return true;
  }
}

module.exports = {
  BenchmarksStream,
};
