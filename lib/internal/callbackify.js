'use strict';

module.exports = callbackify;

function callbackify(fn) {
  Object.defineProperty(inner, 'name', {
    value: fn.name + 'Callback'
  });
  return inner;

  function inner() {
    const cb = arguments[arguments.length - 1];
    if (typeof cb !== 'function') {
      cb = (err) => { throw err; };
    }

    new Promise((resolve) => {
      resolve(fn.apply(this, arguments));
    }).then(
      (result) => cb(null, result),
      (error) => cb(error)
    );
  }
}
