'use strict';

const ObjectGetOwnPropertyDescriptors = require('object.getownpropertydescriptors');
const util = require('util');
const timers = require('timers');

const kCustomPromisifiedSymbol = util.promisify && util.promisify.custom || Symbol('util.promisify.custom');
//const kCustomPromisifyArgsSymbol = Symbol('customPromisifyArgs');

function promisify(orig) {
  if (typeof orig !== 'function') {
    //const errors = require('internal/errors');
    //throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'original', 'function');
    var err = TypeError(`The "original" argument must be of type function`);
    err.code = 'ERR_INVALID_ARG_TYPE';
    err.name = `TypeError [${err.code}]`;
    throw err
  }

  if (orig === timers.setTimeout || orig === timers.setImmediate) {
    const _orig = orig
    orig = function () {
      var args = [];
      for (var i = 0; i < arguments.length; i ++) args.push(arguments[i]);
      const _cb = args.pop()
      const cb = function () {
        var args = [];
        for (var i = 0; i < arguments.length; i ++) args.push(arguments[i]);
        _cb.apply(null, [null].concat(args))
      }
      _orig.apply(timers, [cb].concat(args))
    }
  }

  if (orig[kCustomPromisifiedSymbol]) {
    const fn = orig[kCustomPromisifiedSymbol];
    if (typeof fn !== 'function') {
      throw new TypeError('The [util.promisify.custom] property must be ' +
                          'a function');
    }
    Object.defineProperty(fn, kCustomPromisifiedSymbol, {
      value: fn, enumerable: false, writable: false, configurable: true
    });
    return fn;
  }

  // Names to create an object from in case the callback receives multiple
  // arguments, e.g. ['stdout', 'stderr'] for child_process.exec.
  //const argumentNames = orig[kCustomPromisifyArgsSymbol];

  function fn() {
    var args = [];
    for (var i = 0; i < arguments.length; i ++) args.push(arguments[i]);

    let resolve, reject;
    const promise = new Promise(function (_resolve, _reject) {
      resolve = _resolve;
      reject = _reject;
    });
    try {
      orig.apply(this, args.concat(function (err) {
        var values = [];
        for (var i = 1; i < arguments.length; i++) values.push(arguments[i]);
        if (err) {
          reject(err);
        //} else if (argumentNames !== undefined && values.length > 1) {
        //  const obj = {};
        //  for (var i = 0; i < argumentNames.length; i++)
        //    obj[argumentNames[i]] = values[i];
        //  resolve(obj);
        } else {
          resolve(values[0]);
        }
      }));
    } catch (err) {
      reject(err);
    }
    return promise;
  }

  Object.setPrototypeOf(fn, Object.getPrototypeOf(orig));

  Object.defineProperty(fn, kCustomPromisifiedSymbol, {
    value: fn, enumerable: false, writable: false, configurable: true
  });
  return Object.defineProperties(fn, ObjectGetOwnPropertyDescriptors(orig));
}

promisify.custom = kCustomPromisifiedSymbol;

module.exports = promisify;
