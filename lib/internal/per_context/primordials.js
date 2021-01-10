'use strict';

/* eslint-disable node-core/prefer-primordials */

// This file subclasses and stores the JS builtins that come from the VM
// so that Node.js's builtin modules do not need to later look these up from
// the global proxy, which can be mutated by users.

const {
  defineProperty: ReflectDefineProperty,
  getOwnPropertyDescriptor: ReflectGetOwnPropertyDescriptor,
  ownKeys: ReflectOwnKeys,
} = Reflect;

// TODO(joyeecheung): we can restrict access to these globals in builtin
// modules through the JS linter, for example: ban access such as `Object`
// (which falls back to a lookup in the global proxy) in favor of
// `primordials.Object` where `primordials` is a lexical variable passed
// by the native module compiler.

// `uncurryThis` is equivalent to `func => Function.prototype.call.bind(func)`.
// It is using `bind.bind(call)` to avoid using `Function.prototype.bind`
// and `Function.prototype.call` after it may have been mutated by users.
const { bind, call } = Function.prototype;
const uncurryThis = bind.bind(call);
primordials.uncurryThis = uncurryThis;

function getNewKey(key) {
  return typeof key === 'symbol' ?
    `Symbol${key.description[7].toUpperCase()}${key.description.slice(8)}` :
    `${key[0].toUpperCase()}${key.slice(1)}`;
}

function copyAccessor(dest, prefix, key, { enumerable, get, set }) {
  ReflectDefineProperty(dest, `${prefix}Get${key}`, {
    value: uncurryThis(get),
    enumerable
  });
  if (set !== undefined) {
    ReflectDefineProperty(dest, `${prefix}Set${key}`, {
      value: uncurryThis(set),
      enumerable
    });
  }
}

function copyPropsRenamed(src, dest, prefix) {
  for (const key of ReflectOwnKeys(src)) {
    const newKey = getNewKey(key);
    const desc = ReflectGetOwnPropertyDescriptor(src, key);
    if ('get' in desc) {
      copyAccessor(dest, prefix, newKey, desc);
    } else {
      ReflectDefineProperty(dest, `${prefix}${newKey}`, desc);
    }
  }
}

function copyPropsRenamedBound(src, dest, prefix) {
  for (const key of ReflectOwnKeys(src)) {
    const newKey = getNewKey(key);
    const desc = ReflectGetOwnPropertyDescriptor(src, key);
    if ('get' in desc) {
      copyAccessor(dest, prefix, newKey, desc);
    } else {
      if (typeof desc.value === 'function') {
        desc.value = desc.value.bind(src);
      }
      ReflectDefineProperty(dest, `${prefix}${newKey}`, desc);
    }
  }
}

function copyPrototype(src, dest, prefix) {
  for (const key of ReflectOwnKeys(src)) {
    const newKey = getNewKey(key);
    const desc = ReflectGetOwnPropertyDescriptor(src, key);
    if ('get' in desc) {
      copyAccessor(dest, prefix, newKey, desc);
    } else {
      if (typeof desc.value === 'function') {
        desc.value = uncurryThis(desc.value);
      }
      ReflectDefineProperty(dest, `${prefix}${newKey}`, desc);
    }
  }
}

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
  { name: 'ArrayIterator', original: {
    prototype: Reflect.getPrototypeOf(Array.prototype[Symbol.iterator]()),
  } },
  { name: 'StringIterator', original: {
    prototype: Reflect.getPrototypeOf(String.prototype[Symbol.iterator]()),
  } },
].forEach(({ name, original }) => {
  primordials[name] = original;
  // The static %TypedArray% methods require a valid `this`, but can't be bound,
  // as they need a subclass constructor as the receiver:
  copyPrototype(original, primordials, name);
  copyPrototype(original.prototype, primordials, `${name}Prototype`);
});

/* eslint-enable node-core/prefer-primordials */

const {
  ArrayPrototypeForEach,
  FunctionPrototypeCall,
  Map,
  ObjectFreeze,
  ObjectSetPrototypeOf,
  Set,
  SymbolIterator,
  WeakMap,
  WeakSet,
} = primordials;

// Because these functions are used by `makeSafe`, which is exposed
// on the `primordials` object, it's important to use const references
// to the primordials that they use:
const createSafeIterator = (factory, next) => {
  class SafeIterator {
    constructor(iterable) {
      this._iterator = factory(iterable);
    }
    next() {
      return next(this._iterator);
    }
    [SymbolIterator]() {
      return this;
    }
  }
  ObjectSetPrototypeOf(SafeIterator.prototype, null);
  ObjectFreeze(SafeIterator.prototype);
  ObjectFreeze(SafeIterator);
  return SafeIterator;
};

primordials.SafeArrayIterator = createSafeIterator(
  primordials.ArrayPrototypeSymbolIterator,
  primordials.ArrayIteratorPrototypeNext
);
primordials.SafeStringIterator = createSafeIterator(
  primordials.StringPrototypeSymbolIterator,
  primordials.StringIteratorPrototypeNext
);

const copyProps = (src, dest) => {
  ArrayPrototypeForEach(ReflectOwnKeys(src), (key) => {
    if (!ReflectGetOwnPropertyDescriptor(dest, key)) {
      ReflectDefineProperty(
        dest,
        key,
        ReflectGetOwnPropertyDescriptor(src, key));
    }
  });
};

const makeSafe = (unsafe, safe) => {
  if (SymbolIterator in unsafe.prototype) {
    const dummy = new unsafe();
    let next; // We can reuse the same `next` method.

    ArrayPrototypeForEach(ReflectOwnKeys(unsafe.prototype), (key) => {
      if (!ReflectGetOwnPropertyDescriptor(safe.prototype, key)) {
        const desc = ReflectGetOwnPropertyDescriptor(unsafe.prototype, key);
        if (
          typeof desc.value === 'function' &&
          desc.value.length === 0 &&
          SymbolIterator in (FunctionPrototypeCall(desc.value, dummy) ?? {})
        ) {
          const createIterator = uncurryThis(desc.value);
          next ??= uncurryThis(createIterator(dummy).next);
          const SafeIterator = createSafeIterator(createIterator, next);
          desc.value = function() {
            return new SafeIterator(this);
          };
        }
        ReflectDefineProperty(safe.prototype, key, desc);
      }
    });
  } else {
    copyProps(unsafe.prototype, safe.prototype);
  }
  copyProps(unsafe, safe);

  ObjectSetPrototypeOf(safe.prototype, null);
  ObjectFreeze(safe.prototype);
  ObjectFreeze(safe);
  return safe;
};
primordials.makeSafe = makeSafe;

// Subclass the constructors because we need to use their prototype
// methods later.
// Defining the `constructor` is necessary here to avoid the default
// constructor which uses the user-mutable `%ArrayIteratorPrototype%.next`.
primordials.SafeMap = makeSafe(
  Map,
  class SafeMap extends Map {
    constructor(i) { super(i); } // eslint-disable-line no-useless-constructor
  }
);
primordials.SafeWeakMap = makeSafe(
  WeakMap,
  class SafeWeakMap extends WeakMap {
    constructor(i) { super(i); } // eslint-disable-line no-useless-constructor
  }
);
primordials.SafeSet = makeSafe(
  Set,
  class SafeSet extends Set {
    constructor(i) { super(i); } // eslint-disable-line no-useless-constructor
  }
);
primordials.SafeWeakSet = makeSafe(
  WeakSet,
  class SafeWeakSet extends WeakSet {
    constructor(i) { super(i); } // eslint-disable-line no-useless-constructor
  }
);

ObjectSetPrototypeOf(primordials, null);
ObjectFreeze(primordials);
