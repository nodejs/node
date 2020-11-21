'use strict';

/* eslint-disable node-core/prefer-primordials */

// This file subclasses and stores the JS builtins that come from the VM
// so that Node.js's builtin modules do not need to later look these up from
// the global proxy, which can be mutated by users.

// TODO(joyeecheung): we can restrict access to these globals in builtin
// modules through the JS linter, for example: ban access such as `Object`
// (which falls back to a lookup in the global proxy) in favor of
// `primordials.Object` where `primordials` is a lexical variable passed
// by the native module compiler.

// `uncurryThis` is equivalent to `func => Function.prototype.call.bind(func)`.
// It is using `call.bind(bind, call)` to avoid using `Function.prototype.bind`
// after it may have been mutated by users.
const { bind, call } = Function.prototype;
const uncurryThis = call.bind(bind, call);
primordials.uncurryThis = uncurryThis;

function copyProps(src, dest) {
  for (const key of Reflect.ownKeys(src)) {
    if (!Reflect.getOwnPropertyDescriptor(dest, key)) {
      Reflect.defineProperty(
        dest,
        key,
        Reflect.getOwnPropertyDescriptor(src, key));
    }
  }
}

function copyPropsRenamed(src, dest, prefix) {
  for (const key of Reflect.ownKeys(src)) {
    if (typeof key === 'string') {
      Reflect.defineProperty(
        dest,
        `${prefix}${key[0].toUpperCase()}${key.slice(1)}`,
        Reflect.getOwnPropertyDescriptor(src, key));
    }
  }
}

function copyPropsRenamedBound(src, dest, prefix) {
  for (const key of Reflect.ownKeys(src)) {
    if (typeof key === 'string') {
      const desc = Reflect.getOwnPropertyDescriptor(src, key);
      if (typeof desc.value === 'function') {
        desc.value = desc.value.bind(src);
      }
      Reflect.defineProperty(
        dest,
        `${prefix}${key[0].toUpperCase()}${key.slice(1)}`,
        desc
      );
    }
  }
}

function copyPrototype(src, dest, prefix) {
  for (const key of Reflect.ownKeys(src)) {
    if (typeof key === 'string') {
      const desc = Reflect.getOwnPropertyDescriptor(src, key);
      if (typeof desc.value === 'function') {
        desc.value = uncurryThis(desc.value);
      }
      Reflect.defineProperty(
        dest,
        `${prefix}${key[0].toUpperCase()}${key.slice(1)}`,
        desc);
    }
  }
}

function makeSafe(unsafe, safe) {
  copyProps(unsafe.prototype, safe.prototype);
  copyProps(unsafe, safe);
  Object.setPrototypeOf(safe.prototype, null);
  Object.freeze(safe.prototype);
  Object.freeze(safe);
  return safe;
}
primordials.makeSafe = makeSafe;

// Subclass the constructors because we need to use their prototype
// methods later.
primordials.SafeMap = makeSafe(
  Map,
  class SafeMap extends Map {}
);
primordials.SafeWeakMap = makeSafe(
  WeakMap,
  class SafeWeakMap extends WeakMap {}
);
primordials.SafeSet = makeSafe(
  Set,
  class SafeSet extends Set {}
);
primordials.SafeWeakSet = makeSafe(
  WeakSet,
  class SafeWeakSet extends WeakSet {}
);

// Create copies of the namespace objects
[
  'JSON',
  'Math',
  'Reflect'
].forEach((name) => {
  copyPropsRenamed(global[name], primordials, name);
});

// Create copies of intrinsic objects
[
  'Array',
  'ArrayBuffer',
  'BigInt',
  'BigInt64Array',
  'BigUint64Array',
  'Boolean',
  'DataView',
  'Date',
  'Error',
  'EvalError',
  'Float32Array',
  'Float64Array',
  'Function',
  'Int16Array',
  'Int32Array',
  'Int8Array',
  'Map',
  'Number',
  'Object',
  'RangeError',
  'ReferenceError',
  'RegExp',
  'Set',
  'String',
  'Symbol',
  'SyntaxError',
  'TypeError',
  'URIError',
  'Uint16Array',
  'Uint32Array',
  'Uint8Array',
  'Uint8ClampedArray',
  'WeakMap',
  'WeakSet',
].forEach((name) => {
  const original = global[name];
  primordials[name] = original;
  copyPropsRenamed(original, primordials, name);
  copyPrototype(original.prototype, primordials, `${name}Prototype`);
});

// Create copies of intrinsic objects that require a valid `this` to call
// static methods.
// Refs: https://www.ecma-international.org/ecma-262/#sec-promise.all
[
  'Promise',
].forEach((name) => {
  const original = global[name];
  primordials[name] = original;
  copyPropsRenamedBound(original, primordials, name);
  copyPrototype(original.prototype, primordials, `${name}Prototype`);
});

// Create copies of abstract intrinsic objects that are not directly exposed
// on the global object.
// Refs: https://tc39.es/ecma262/#sec-%typedarray%-intrinsic-object
[
  { name: 'TypedArray', original: Reflect.getPrototypeOf(Uint8Array) },
].forEach(({ name, original }) => {
  primordials[name] = original;
  // The static %TypedArray% methods require a valid `this`, but can't be bound,
  // as they need a subclass constructor as the receiver:
  copyPrototype(original, primordials, name);
  copyPrototype(original.prototype, primordials, `${name}Prototype`);
});

Object.setPrototypeOf(primordials, null);
Object.freeze(primordials);
