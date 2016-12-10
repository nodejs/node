
/**
 * Module dependencies.
 */

var inherits = require('util').inherits;
var delimiter = require('path').delimiter || ':';
var ArrayIndex = require('array-index');

/**
 * Module exports.
 */

module.exports = PathArray;

/**
 * `PathArray` constructor. Treat your `$PATH` like a mutable JavaScript Array!
 *
 * @param {Env} env - `process.env` object to use.
 * @param {String} [property] - optional property name to use (`PATH` by default).
 * @public
 */

function PathArray (env, property) {
  if (!(this instanceof PathArray)) return new PathArray(env);
  ArrayIndex.call(this);

  this.property = property || 'PATH';

  // overwrite only the `get` operator of the ".length" property
  Object.defineProperty(this, 'length', {
    get: this._getLength
  });

  // store the `process.env` object as a non-enumerable `_env`
  Object.defineProperty(this, '_env', {
    value: env || process.env,
    writable: true,
    enumerable: false,
    configurable: true
  });

  // need to invoke the `length` getter to ensure that the
  // indexed getters/setters are set up at this point
  void(this.length);
}

// inherit from ArrayIndex
inherits(PathArray, ArrayIndex);

/**
 * Returns the current $PATH representation as an Array.
 *
 * @api private
 */

PathArray.prototype._array = function () {
  var path = this._env[this.property];
  if (!path) return [];
  return path.split(delimiter);
};

/**
 * Sets the `env` object's `PATH` string to the values in the passed in Array
 * instance.
 *
 * @api private
 */

PathArray.prototype._setArray = function (arr) {
  // mutate the $PATH
  this._env[this.property] = arr.join(delimiter);
};

/**
 * `.length` getter operation implementation.
 *
 * @api private
 */

PathArray.prototype._getLength = function () {
  var length = this._array().length;

  // invoke the ArrayIndex internal `set` operator to ensure that
  // there's getters/setters defined for the determined length so far...
  this.length = length;

  return length;
};

/**
 * ArrayIndex [0] getter operator implementation.
 *
 * @api private
 */

PathArray.prototype[ArrayIndex.get] = function get (index) {
  return this._array()[index];
};

/**
 * ArrayIndex [0]= setter operator implementation.
 *
 * @api private
 */

PathArray.prototype[ArrayIndex.set] = function set (index, value) {
  var arr = this._array();
  arr[index] = value;
  this._setArray(arr);
  return value;
};

/**
 * `toString()` returns the current $PATH string.
 *
 * @api public
 */

PathArray.prototype.toString = function toString () {
  return this._env[this.property] || '';
};

// proxy the JavaScript Array functions, and mutate the $PATH
Object.getOwnPropertyNames(Array.prototype).forEach(function (name) {
  if ('constructor' == name) return;
  if ('function' != typeof Array.prototype[name]) return;
  if (/to(Locale)?String/.test(name)) return;
  //console.log('proxy %s', name);

  PathArray.prototype[name] = function () {
    var arr = this._array();
    var rtn = arr[name].apply(arr, arguments);
    this._setArray(arr);
    return rtn;
  };
});
