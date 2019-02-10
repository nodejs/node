'use strict';

/* global breakAtBootstrap, primordials */

// This file subclasses and stores the JS builtins that come from the VM
// so that Node.js's builtin modules do not need to later look these up from
// the global proxy, which can be mutated by users.

// TODO(joyeecheung): we can restrict access to these globals in builtin
// modules through the JS linter, for example: ban access such as `Object`
// (which falls back to a lookup in the global proxy) in favor of
// `primordials.Object` where `primordials` is a lexical variable passed
// by the native module compiler.

if (breakAtBootstrap) {
  debugger;  // eslint-disable-line no-debugger
}

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

function makeSafe(unsafe, safe) {
  copyProps(unsafe.prototype, safe.prototype);
  copyProps(unsafe, safe);
  Object.setPrototypeOf(safe.prototype, null);
  Object.freeze(safe.prototype);
  Object.freeze(safe);
  return safe;
}

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
primordials.SafePromise = makeSafe(
  Promise,
  class SafePromise extends Promise {}
);

// Create copies of the namespace objects
[
  'JSON',
  'Math',
  'Reflect'
].forEach((name) => {
  const target = primordials[name] = Object.create(null);
  copyProps(global[name], target);
});

// Create copies of intrinsic objects
[
  'Array',
  'Date',
  'Function',
  'Object',
  'RegExp',
  'String',
  'Symbol',
].forEach((name) => {
  const target = primordials[name] = Object.create(null);
  copyProps(global[name], target);
  const proto = primordials[name + 'Prototype'] = Object.create(null);
  copyProps(global[name].prototype, proto);
});

Object.setPrototypeOf(primordials, null);
Object.freeze(primordials);
