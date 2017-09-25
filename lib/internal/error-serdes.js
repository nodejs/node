'use strict';

const Buffer = require('buffer').Buffer;
const { serialize, deserialize } = require('v8');
const { SafeSet } = require('internal/safe_globals');

const kSerializedError = 0;
const kSerializedObject = 1;
const kInspectedError = 2;

const GetPrototypeOf = Object.getPrototypeOf;
const GetOwnPropertyDescriptor = Object.getOwnPropertyDescriptor;
const GetOwnPropertyNames = Object.getOwnPropertyNames;
const DefineProperty = Object.defineProperty;
const Assign = Object.assign;
const ObjectPrototypeToString =
    Function.prototype.call.bind(Object.prototype.toString);
const ForEach = Function.prototype.call.bind(Array.prototype.forEach);
const Call = Function.prototype.call.bind(Function.prototype.call);

const errors = {
  Error, TypeError, RangeError, URIError, SyntaxError, ReferenceError, EvalError
};
const errorConstructorNames = new SafeSet(Object.keys(errors));

function TryGetAllProperties(object, target = object) {
  const all = Object.create(null);
  if (object === null)
    return all;
  Assign(all, TryGetAllProperties(GetPrototypeOf(object), target));
  const keys = GetOwnPropertyNames(object);
  ForEach(keys, (key) => {
    const descriptor = GetOwnPropertyDescriptor(object, key);
    const getter = descriptor.get;
    if (getter && key !== '__proto__') {
      try {
        descriptor.value = Call(getter, target);
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
    current = GetPrototypeOf(current)) {
    const desc = GetOwnPropertyDescriptor(current, 'constructor');
    if (desc && desc.value) {
      DefineProperty(constructors, constructors.length, {
        value: desc.value, enumerable: true
      });
    }
  }

  return constructors;
}

function GetName(object) {
  const desc = GetOwnPropertyDescriptor(object, 'name');
  return desc && desc.value;
}

let util;
function lazyUtil() {
  if (!util)
    util = require('util');
  return util;
}

function serializeError(error) {
  try {
    if (typeof error === 'object' &&
        ObjectPrototypeToString(error) === '[object Error]') {
      const constructors = GetConstructors(error);
      for (var i = constructors.length - 1; i >= 0; i--) {
        const name = GetName(constructors[i]);
        if (errorConstructorNames.has(name)) {
          try { error.stack; } catch {}
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
                        Buffer.from(lazyUtil().inspect(error), 'utf8')]);
}

function deserializeError(error) {
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
