'use strict';

const SUFFIX = 'Promise';
const SHORTCUT_NAME = 'promised';
const install = process._enablePromises ? _install : _noop;

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
  return _promisify(object, name, (err, arg0, arg1) => {
    if (err) {
      throw err;
    }
    return {
      [mapped[0]]: arg0,
      [mapped[1]]: arg1
    };
  });
}

function _promisify(object, name, handler) {
  const fn = install(object, name, inner);

  function inner() {
    const args = Array.from(arguments);
    return new Promise((resolve, reject) => {
      fn.call(this, ...args, resolver);

      function resolver(arg0, arg1, arg2) {
        try {
          resolve(handler(arg0, arg1, arg2));
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

  if (!object[SHORTCUT_NAME]) {
    object[SHORTCUT_NAME] = {};
  }
  object[SHORTCUT_NAME][name] = inner;
  return fn;
}

function _noop(object, name, inner) {
}
