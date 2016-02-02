'use strict';

module.exports = promisify;

function promisify(fn, single) {
  Object.defineProperty(inner, 'name', {
    value: fn.name + 'Promisified'
  });
  return inner;

  function inner() {
    if (typeof arguments[arguments.length - 1] === 'function') {
      return fn.apply(this, arguments);
    }
    var resolve = null;
    var reject = null;
    const args = Array.from(arguments);
    const promise = new Promise((_resolve, _reject) => {
      resolve = _resolve;
      reject = _reject;
    });

    try {
      args.push(single ? resolve : resolver);
      fn.apply(this, args);
    } catch (err) {
      reject(err);
    }
    return promise;

    function resolver(err) {
      if (err) {
        return reject(err);
      }
      switch (arguments.length) {
        case 0: return resolve();
        case 1: return resolve();
        case 2: return resolve(arguments[1]);
      }
      return resolve(Array.from(arguments).slice(1));
    }
  }
}
