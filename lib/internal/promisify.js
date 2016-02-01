'use strict';

module.exports = promisify;

function promisify (fn) {
  Object.defineProperty(inner, 'name', {
    writable: false,
    enumerable: false,
    configurable: true,
    value: fn.name + 'Promisified'
  });
  return inner;

  function inner () {
    if (typeof arguments[arguments.length - 1] === 'function') {
      return fn.apply(this, arguments);
    }
    var resolve = null;
    var reject = null;
    const promise = new Promise((_resolve, _reject) => {
      resolve = _resolve;
      reject = _reject;
    });
    try {
      fn.apply(this, [...arguments, function (err) {
        if (err) {
          return reject(err);
        }
        switch (arguments.length) {
          case 0: return resolve();
          case 1: return resolve();
          case 2: return resolve(arguments[1]);
        }
        return resolve(resolver(...[...arguments].slice(1)));
      }]);
    } catch (err) {
      reject(err);
    }
    return promise;
  }
}
