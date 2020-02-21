'use strict';

const Buffer = require('buffer').Buffer;
const {
  ArrayPrototypeForEach,
  Error,
  FunctionPrototypeCall,
  ObjectAssign,
  ObjectCreate,
  ObjectDefineProperty,
  ObjectGetOwnPropertyDescriptor,
  ObjectGetOwnPropertyNames,
  ObjectGetPrototypeOf,
  ObjectKeys,
  ObjectPrototypeToString,
  SafeSet,
} = primordials;

const kSerializedError = 0;
const kSerializedObject = 1;
const kInspectedError = 2;

const errors = {
  Error, TypeError, RangeError, URIError, SyntaxError, ReferenceError, EvalError
};
const errorConstructorNames = new SafeSet(ObjectKeys(errors));

function TryGetAllProperties(object, target = object) {
  const all = ObjectCreate(null);
  if (object === null)
    return all;
  ObjectAssign(all,
               TryGetAllProperties(ObjectGetPrototypeOf(object), target));
  const keys = ObjectGetOwnPropertyNames(object);
  ArrayPrototypeForEach(keys, (key) => {
    let descriptor;
    try {
      descriptor = ObjectGetOwnPropertyDescriptor(object, key);
    } catch { return; }
    const getter = descriptor.get;
    if (getter && key !== '__proto__') {
      try {
        descriptor.value = FunctionPrototypeCall(getter, target);
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

  for (let current = object;
    current !== null;
    current = ObjectGetPrototypeOf(current)) {
    const desc = ObjectGetOwnPropertyDescriptor(current, 'constructor');
    if (desc && desc.value) {
      ObjectDefineProperty(constructors, constructors.length, {
        value: desc.value, enumerable: true
      });
    }
  }

  return constructors;
}

function GetName(object) {
  const desc = ObjectGetOwnPropertyDescriptor(object, 'name');
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
        ObjectPrototypeToString(error) === '[object Error]') {
      const constructors = GetConstructors(error);
      for (let i = 0; i < constructors.length; i++) {
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
      return ObjectCreate(ctor.prototype, properties);
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
