'use strict';

module.exports = promisify;

const promises = require('internal/promises');
const BuiltinPromise = promises.Promise;

function promisify (fn, single) {
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
    const promise = promises.wrap(new BuiltinPromise((_resolve, _reject) => {
      resolve = _resolve;
      reject = _reject;
    }));

    fn.apply(this, [...arguments, single ? resolve : function (err) {
      if (err) {
        return reject(err);
      }
      switch (arguments.length) {
        case 0: return resolve();
        case 1: return resolve();
        case 2: return resolve(arguments[1]);
      }
      return resolve([...arguments].slice(1));
    }]);
    return promise;
  }
}
