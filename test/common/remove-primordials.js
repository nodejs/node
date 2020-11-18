/* eslint-disable node-core/require-common-first, node-core/required-modules */
'use strict';

const internal_percontext_length = 'internal/per_context/'.length;
const isInternalUnsafeCall = (error) => {
  // This function is equivalent to
  // /\(node:(?!.{21}callunsafe)/.test(error.stack?.split('\n')[2] || '')
  let i, n;
  const stack = error.stack || '';
  const { length } = stack;
  for (i = 0, n = 0; i < length && !(n === 2 && stack[i] === '('); i++) {
    // Iterate over string until we find an open parenthesis on line #2.
    if (stack[i] === '\n' && n++ === 2)
      // Shortcut if there is no parenthesis on line #2.
      return false;
  }
  return i !== length &&
         i + 5 < length &&
         stack[++i] === 'n' &&
         stack[++i] === 'o' &&
         stack[++i] === 'd' &&
         stack[++i] === 'e' &&
         stack[++i] === ':' &&
         (i + internal_percontext_length + 10 > length || (
           stack[internal_percontext_length + ++i] !== 'c' &&
           stack[internal_percontext_length + ++i] !== 'a' &&
           stack[internal_percontext_length + ++i] !== 'l' &&
           stack[internal_percontext_length + ++i] !== 'l' &&
           stack[internal_percontext_length + ++i] !== 'u' &&
           stack[internal_percontext_length + ++i] !== 'n' &&
           stack[internal_percontext_length + ++i] !== 's' &&
           stack[internal_percontext_length + ++i] !== 'a' &&
           stack[internal_percontext_length + ++i] !== 'f' &&
           stack[internal_percontext_length + ++i] !== 'e'));
};


const toDelete = [];
function getMethodsFromPrototype(src, name) {
  for (const key of Reflect.ownKeys(src)) {
    if (key === 'toString') continue;
    if (typeof key === 'string') {
      const desc = Reflect.getOwnPropertyDescriptor(src, key);
      if (typeof desc.value === 'function') {
        const primordialName =
          name === 'Function' && (key === 'apply' || key === 'call') ?
            'ReflectApply' :
            `${name}Prototype${key[0].toUpperCase()}${key.slice(1)}`;
        toDelete.push([src, key, primordialName]);
      }
    }
  }
}

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
  'Promise',
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
  getMethodsFromPrototype(globalThis[name].prototype, name);
});

[
  { name: 'TypedArray', original: Reflect.getPrototypeOf(Uint8Array) },
].forEach(({ name, original }) => {
  getMethodsFromPrototype(original.prototype, name);
});

for (const [proto, methodName, primordial] of toDelete) {
  const errorMessage =
    `Unsafe use of \`<object>.${methodName}(...<arguments>)\`.` +
    ` Use \`primordials.${primordial}(<object>, ...<arguments>)\` instead,` +
    ` or \`callUnsafe(<object>, '${methodName}', ...<arguments>)\`.`;
  const original = proto[methodName];
  proto[methodName] =
    function() {
      const error = new Error(errorMessage);
      if (isInternalUnsafeCall(error)) {
        throw error;
      } else {
        return Reflect.apply(original, this, arguments);
      }
    };
}
