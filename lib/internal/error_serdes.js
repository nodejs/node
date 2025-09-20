'use strict';

const {
  ArrayPrototypeForEach,
  Error,
  EvalError,
  FunctionPrototypeCall,
  ObjectAssign,
  ObjectDefineProperties,
  ObjectDefineProperty,
  ObjectGetOwnPropertyDescriptor,
  ObjectGetOwnPropertyNames,
  ObjectGetPrototypeOf,
  ObjectKeys,
  ObjectPrototypeToString,
  RangeError,
  ReferenceError,
  ReflectConstruct,
  ReflectDeleteProperty,
  SafeSet,
  StringFromCharCode,
  StringPrototypeSubstring,
  SymbolFor,
  SyntaxError,
  TypeError,
  TypedArrayPrototypeGetBuffer,
  TypedArrayPrototypeGetByteLength,
  TypedArrayPrototypeGetByteOffset,
  URIError,
} = primordials;

const assert = require('internal/assert');

const { Buffer } = require('buffer');
const { inspect: { custom: customInspectSymbol } } = require('util');

const kSerializedError = 0;
const kSerializedObject = 1;
const kInspectedError = 2;
const kInspectedSymbol = 3;
const kCustomInspectedObject = 4;
const kCircularReference = 5;

const kSymbolStringLength = 'Symbol('.length;

// TODO: implement specific logic for AggregateError/SuppressedError
const errors = {
  Error, TypeError, RangeError, URIError, SyntaxError, ReferenceError, EvalError,
};
const errorConstructorNames = new SafeSet(ObjectKeys(errors));

function TryGetAllProperties(object, target = object, rememberSet) {
  const all = { __proto__: null };
  if (object === null)
    return all;
  ObjectAssign(all,
               TryGetAllProperties(ObjectGetPrototypeOf(object), target, rememberSet));
  const keys = ObjectGetOwnPropertyNames(object);
  ArrayPrototypeForEach(keys, (key) => {
    let descriptor;
    try {
      // TODO: create a null-prototype descriptor with needed properties only
      descriptor = ObjectGetOwnPropertyDescriptor(object, key);
    } catch { return; }
    const getter = descriptor.get;
    if (getter && key !== '__proto__') {
      try {
        descriptor.value = FunctionPrototypeCall(getter, target);
        delete descriptor.get;
        delete descriptor.set;
      } catch {
        // Continue regardless of error.
      }
    }
    if (key === 'cause') {
      descriptor.value = serializeError(descriptor.value, rememberSet);
      all[key] = descriptor;
    } else if ('value' in descriptor &&
            typeof descriptor.value !== 'function' && typeof descriptor.value !== 'symbol') {
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
    if (desc?.value) {
      ObjectDefineProperty(constructors, constructors.length, {
        __proto__: null,
        value: desc.value, enumerable: true,
      });
    }
  }

  return constructors;
}

function GetName(object) {
  const desc = ObjectGetOwnPropertyDescriptor(object, 'name');
  return desc?.value;
}

let internalUtilInspect;
function inspect(...args) {
  internalUtilInspect ??= require('internal/util/inspect');
  return internalUtilInspect.inspect(...args);
}

let serialize;
function serializeError(error, rememberSet = new SafeSet()) {
  serialize ??= require('v8').serialize;
  if (typeof error === 'symbol') {
    return Buffer.from(StringFromCharCode(kInspectedSymbol) + inspect(error), 'utf8');
  }

  try {
    if (typeof error === 'object' &&
        ObjectPrototypeToString(error) === '[object Error]') {
      if (rememberSet.has(error)) {
        return Buffer.from([kCircularReference]);
      }
      rememberSet.add(error);

      const constructors = GetConstructors(error);
      for (let i = 0; i < constructors.length; i++) {
        const name = GetName(constructors[i]);
        if (errorConstructorNames.has(name)) {
          const serialized = serialize({
            constructor: name,
            properties: TryGetAllProperties(error, error, rememberSet),
          });
          return Buffer.concat([Buffer.from([kSerializedError]), serialized]);
        }
      }
    }
  } catch {
    // Continue regardless of error.
  }
  try {
    if (error != null && customInspectSymbol in error) {
      return Buffer.from(StringFromCharCode(kCustomInspectedObject) + inspect(error), 'utf8');
    }
  } catch {
    // Continue regardless of error.
  }
  try {
    const serialized = serialize(error);
    return Buffer.concat([Buffer.from([kSerializedObject]), serialized]);
  } catch {
    // Continue regardless of error.
  }
  return Buffer.from(StringFromCharCode(kInspectedError) + inspect(error), 'utf8');
}

function fromBuffer(error) {
  return Buffer.from(TypedArrayPrototypeGetBuffer(error),
                     TypedArrayPrototypeGetByteOffset(error) + 1,
                     TypedArrayPrototypeGetByteLength(error) - 1);
}

let deserialize;
function deserializeError(error) {
  deserialize ??= require('v8').deserialize;
  switch (error[0]) {
    case kSerializedError: {
      const { constructor, properties } = deserialize(error.subarray(1));
      assert(errorConstructorNames.has(constructor), 'Invalid constructor');
      if ('cause' in properties && 'value' in properties.cause) {
        properties.cause.value = deserializeError(properties.cause.value);
      }
      // Invoke the Error constructor to gain an object with an [[ErrorData]] internal slot
      const ret = ReflectConstruct(Error, [], errors[constructor]);
      // Delete any properties defined by the Error constructor before assigning from source
      ArrayPrototypeForEach(ObjectGetOwnPropertyNames(ret), (key) => {
        ReflectDeleteProperty(ret, key);
      });
      return ObjectDefineProperties(ret, properties);
    }
    case kSerializedObject:
      return deserialize(error.subarray(1));
    case kInspectedError:
      return fromBuffer(error).toString('utf8');
    case kInspectedSymbol: {
      const buf = fromBuffer(error);
      return SymbolFor(StringPrototypeSubstring(buf.toString('utf8'), kSymbolStringLength, buf.length - 1));
    }
    case kCustomInspectedObject:
      return {
        __proto__: null,
        [customInspectSymbol]: () => fromBuffer(error).toString('utf8'),
      };
    case kCircularReference:
      return {
        __proto__: null,
        [customInspectSymbol]: () => '[Circular object]',
      };
  }
  assert.fail('Unknown serializer flag');
}

module.exports = { serializeError, deserializeError };
