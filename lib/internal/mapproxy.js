'use strict';

// A proxy object implementation that wraps a Map and makes it look and
// act a close as possible to a dictionary-type object.

const handler = {
  getPrototypeOf(target) {
    return Object.getPrototypeOf(target);
  },
  isExtensible(target) {
    return true;
  },
  getOwnPropertyDescriptor(target, prop) {
    if (!target.has(prop))
      return;
    return {
      enumerable: true,
      configurable: true,
      writable: true,
      value: target.get(prop)
    };
  },
  defineProperty(target, prop, descriptor) {
    // Properties cannot be defined on these objects
    // because the semantics do not carry through to
    // the underlying Map appropriately.
    throw new TypeError(
      'Object.defineProperty is unsupported on this object.');
  },
  has(target, prop) {
    return target.has(prop);
  },
  get(target, prop) {
    if (target.has(prop))
      return target.get(prop);
    var fn = Object.prototype[prop];
    if (typeof fn === 'function') {
      return new Proxy(fn, {
        apply(target, thisArg, argumentsList) {
          return fn.apply(thisArg, argumentsList);
        }
      });
    }
    return fn;
  },
  set(target, prop, value) {
    target.set(prop, value);
    return true;
  },
  deleteProperty(target, prop) {
    target.delete(prop);
    return true;
  },
  ownKeys(target) {
    return Array.from(target.keys());
  }
};

function wrap(map) {
  return new Proxy(map, handler);
}

exports.wrap = wrap;
