'use strict';

module.exports = callbackify;

function callbackify (fn) {
  Object.defineProperty(inner, 'name', {
    writable: false,
    enumerable: false,
    configurable: true,
    value: fn.name + 'Callback'
  });
  return inner;

  function inner () {
    var resolve = null;
    var reject = null;
    const cb = arguments[arguments.length - 1];
    if (typeof cb !== 'function') {
      cb = err => { throw err; };
    }
    const promise = new Promise((_resolve, _reject) => {
      resolve = _resolve;
      reject = _reject;
    });
    try {
      Promise.resolve(fn.apply(this, arguments)).then(
        result => cb(null, result),
        error => cb(error)
      );
    } catch (err) {
      cb(err);
    }
  }
}
