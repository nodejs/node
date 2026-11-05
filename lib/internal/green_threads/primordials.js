'use strict';
/* eslint-disable node-core/prefer-primordials, no-restricted-globals */

// The primordials of green threads: the part of Node.js' primordials (see
// lib/internal/per_context/primordials.js) that the green thread runtime and
// internal/per_context/domexception use, with the same names and semantics.
// Creating all of Node.js' primordials takes about 3 ms, most of the time it
// takes to start a green thread. Add what the runtime uses here.

const {
  defineProperty: ReflectDefineProperty,
  getOwnPropertyDescriptor: ReflectGetOwnPropertyDescriptor,
  ownKeys: ReflectOwnKeys,
} = Reflect;
const { bind, call } = Function.prototype;
const uncurryThis = bind.bind(call);

function define(name, value) {
  ReflectDefineProperty(primordials, name, {
    __proto__: null,
    value,
    writable: false,
    enumerable: false,
    configurable: false,
  });
}

// Copied as they are.
for (const name of [
  'Array',
  'Error',
  'FinalizationRegistry',
  'Map',
  'Promise',
  'RangeError',
  'Set',
  'Symbol',
  'TypeError',
  'WeakMap',
  'WeakRef',
]) {
  define(name, globalThis[name]);
}
define('globalThis', globalThis);

// `${Global}${Member}`. Functions are called with their global as `this`.
for (const { 0: global, 1: members } of [
  ['Array', ['isArray']],
  ['Date', ['now']],
  ['Number', ['isFinite', 'isInteger', 'isNaN']],
  ['Object', [
    'defineProperties',
    'defineProperty',
    'freeze',
    'getPrototypeOf',
    'keys',
    'setPrototypeOf',
  ]],
  ['Promise', ['reject', 'resolve', 'withResolvers']],
  ['Reflect', ['apply', 'construct']],
  ['Symbol', ['dispose', 'iterator', 'toStringTag']],
]) {
  const target = globalThis[global];
  for (const member of members) {
    let value = target[member];
    if (typeof value === 'function') value = bind.call(value, target);
    define(`${global}${member[0].toUpperCase()}${member.slice(1)}`, value);
  }
}

// `${Global}Prototype`, and `${Global}Prototype${Method}`, which takes the
// receiver as its first argument.
for (const { 0: global, 1: methods } of [
  ['Array', ['findIndex', 'forEach', 'push', 'slice', 'splice']],
  ['Error', []],
  ['Function', ['call']],
  ['Map', ['delete', 'get', 'set']],
  ['Promise', ['then']],
]) {
  const prototype = globalThis[global].prototype;
  define(`${global}Prototype`, prototype);
  for (const method of methods) {
    define(`${global}Prototype${method[0].toUpperCase()}${method.slice(1)}`,
           uncurryThis(prototype[method]));
  }
}

// Collections that user code can't change the behavior of. Like Node.js'
// makeSafe(), whose iterator wrapping never applies to them.
const makeSafe = (unsafe, safe) => {
  for (const { 0: src, 1: dest } of [[unsafe.prototype, safe.prototype],
                                     [unsafe, safe]]) {
    for (const key of ReflectOwnKeys(src)) {
      if (!ReflectGetOwnPropertyDescriptor(dest, key)) {
        ReflectDefineProperty(
          dest,
          key,
          { __proto__: null, ...ReflectGetOwnPropertyDescriptor(src, key) });
      }
    }
  }
  Object.setPrototypeOf(safe.prototype, null);
  Object.freeze(safe.prototype);
  Object.freeze(safe);
  return safe;
};

define('SafeMap', makeSafe(Map, class SafeMap extends Map {}));
define('SafeSet', makeSafe(Set, class SafeSet extends Set {}));
define('SafeWeakMap', makeSafe(WeakMap, class SafeWeakMap extends WeakMap {}));
define('SafeFinalizationRegistry', makeSafe(
  FinalizationRegistry,
  class SafeFinalizationRegistry extends FinalizationRegistry {},
));
define('SafeWeakRef', makeSafe(WeakRef, class SafeWeakRef extends WeakRef {}));

Object.freeze(primordials);
