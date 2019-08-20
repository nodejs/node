'use strict';

const Buffer = require('buffer').Buffer;
const {
  ArrayPrototype,
  FunctionPrototype,
  Object,
  ObjectPrototype,
  SafeSet,
} = primordials;

const kSerializedError = 0;
const kSerializedObject = 1;
const kInspectedError = 2;

const errors = {
  Error, TypeError, RangeError, URIError, SyntaxError, ReferenceError, EvalError
};
const errorConstructorNames = new SafeSet(Object.keys(errors));

function TryGetAllProperties(object, target = object) {
  const all = Object.create(null);
  if (object === null)
    return all;
  Object.assign(all,
                TryGetAllProperties(Object.getPrototypeOf(object), target));
  const keys = Object.getOwnPropertyNames(object);
  ArrayPrototype.forEach(keys, (key) => {
    let descriptor;
    try {
      descriptor = Object.getOwnPropertyDescriptor(object, key);
    } catch { return; }
    const getter = descriptor.get;
    if (getter && key !== '__proto__') {
      try {
        descriptor.value = FunctionPrototype.call(getter, target);
      } catch {}
    }
    if ('value' in descriptor && typeof descriptor.value !== 'function') {
      delete descriptor.get;
      delete descriptor.set;
      all[key] = descriptor;
    }
  });
  return all;
}

function GetConstructors(object) {
  const constructors = [];

  for (var current = object;
    current !== null;
    current = Object.getPrototypeOf(current)) {
    const desc = Object.getOwnPropertyDescriptor(current, 'constructor');
    if (desc && desc.value) {
      Object.defineProperty(constructors, constructors.length, {
        value: desc.value, enumerable: true
      });
    }
  }

  return constructors;
}

function GetName(object) {
  const desc = Object.getOwnPropertyDescriptor(object, 'name');
  return desc && desc.value;
}

let internalUtilInspect;
function inspect(...args) {
  if (!internalUtilInspect) {
    internalUtilInspect = require('internal/util/inspect');
  }
  return internalUtilInspect.inspect(...args);
}

let serialize;
function serializeError(error) {
  if (!serialize) serialize = require('v8').serialize;
  try {
    if (typeof error === 'object' &&
        ObjectPrototype.toString(error) === '[object Error]') {
      const constructors = GetConstructors(error);
      for (var i = 0; i < constructors.length; i++) {
        const name = GetName(constructors[i]);
        if (errorConstructorNames.has(name)) {
          const serialized = serialize({
            constructor: name,
            properties: TryGetAllProperties(error)
          });
          return Buffer.concat([Buffer.from([kSerializedError]), serialized]);
        }
      }
    }
  } catch {}
  try {
    const serialized = serialize(error);
    return Buffer.concat([Buffer.from([kSerializedObject]), serialized]);
  } catch {}
  return Buffer.concat([Buffer.from([kInspectedError]),
                        Buffer.from(inspect(error), 'utf8')]);
}

let deserialize;
function deserializeError(error) {
  if (!deserialize) deserialize = require('v8').deserialize;
  switch (error[0]) {
    case kSerializedError:
      const { constructor, properties } = deserialize(error.subarray(1));
      const ctor = errors[constructor];
      return Object.create(ctor.prototype, properties);
    case kSerializedObject:
      return deserialize(error.subarray(1));
    case kInspectedError:
      const buf = Buffer.from(error.buffer,
                              error.byteOffset + 1,
                              error.byteLength - 1);
      return buf.toString('utf8');
  }
  require('assert').fail('This should not happen');
}

module.exports = { serializeError, deserializeError };
