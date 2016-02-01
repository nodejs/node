'use strict';

module.exports = callbackify;

const promises = require('internal/promises');
const BuiltinPromise = promises.Promise;

function callbackify(fn) {
  Object.defineProperty(inner, 'name', {
    writable: false,
    enumerable: false,
    configurable: true,
    value: fn.name + 'Callback'
  });
  return inner;

  function inner() {
    const cb = arguments[arguments.length - 1];
    if (typeof cb !== 'function') {
      cb = (err) => { throw err; };
    }
    try {
      BuiltinPromise.resolve(fn.apply(this, arguments)).then(
        (result) => cb(null, result),
        (error) => cb(error)
      );
    } catch (err) {
      cb(err);
    }
  }
}
