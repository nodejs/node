'use strict';

const SUFFIX = '$'

const install = process._enablePromises ? _install : _noop

module.exports = {
  normal,
  single,
  multiple
};

function normal(object, name) {
  return _promisify(object, name, (err, val) => {
    if (err) {
      throw err;
    }
    return val;
  });
}

function single(object, name) {
  return _promisify(object, name, (val) => {
    return val;
  });
}

function multiple(object, name, mapped) {
  return _promisify(object, name, (err) => {
    if (err) {
      throw err;
    }
    const output = {};
    for (var i = 0; i < mapped.length; ++i) {
      output[mapped[i]] = arguments[i + 1];
    }
    return output;
  });
}

function _promisify(object, name, handler) {
  install(object, name, inner);

  function inner() {
    return new Promise((resolve, reject) => {
      // this might be slower than array manipulation, but it might not be.
      fn.call(this, ...arguments, single ? resolve : resolver);

      function resolver() {
        try {
          resolve(handler(Array.from(arguments)));
        } catch (err) {
          reject(err);
        }
      }
    });
  }
}

function _install(object, name, inner) {
  const fn = object[name];
  Object.defineProperty(inner, 'name', {
    value: (fn.name || name) + 'Promisified'
  });
  Object.defineProperty(object, `${name}${SUFFIX}`, {
    enumerable: false,
    value: inner
  });
}

function _noop(object, name, inner) {
}
