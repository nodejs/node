
/**
 * Module dependencies.
 */

var Symbol = require('es6-symbol');
var debug = require('debug')('array-index');

var get = Symbol('get');
var set = Symbol('set');
var length = Symbol('length');

/**
 * JavaScript Array "length" is bound to an unsigned 32-bit int.
 * See: http://stackoverflow.com/a/6155063/376773
 */

var MAX_LENGTH = Math.pow(2, 32);

/**
 * Module exports.
 */

module.exports = ArrayIndex;
ArrayIndex.get = get;
ArrayIndex.set = set;

/**
 * Subclass this.
 */

function ArrayIndex (_length) {
  Object.defineProperty(this, 'length', {
    get: getLength,
    set: setLength,
    enumerable: false,
    configurable: true
  });

  this[length] = 0;

  if (arguments.length > 0) {
    setLength.call(this, _length);
  }
}

/**
 * You overwrite the "get" Symbol in your subclass.
 */

ArrayIndex.prototype[ArrayIndex.get] = function () {
  throw new Error('you must implement the `ArrayIndex.get` Symbol');
};

/**
 * You overwrite the "set" Symbol in your subclass.
 */

ArrayIndex.prototype[ArrayIndex.set] = function () {
  throw new Error('you must implement the `ArrayIndex.set` Symbol');
};

/**
 * Converts this array class into a real JavaScript Array. Note that this
 * is a "flattened" array and your defined getters and setters won't be invoked
 * when you interact with the returned Array. This function will call the
 * getter on every array index of the object.
 *
 * @return {Array} The flattened array
 * @api public
 */

ArrayIndex.prototype.toArray = function toArray () {
  var i = 0;
  var l = this.length;
  var array = new Array(l);
  for (; i < l; i++) {
    array[i] = this[i];
  }
  return array;
};

/**
 * Basic support for `JSON.stringify()`.
 */

ArrayIndex.prototype.toJSON = function toJSON () {
  return this.toArray();
};

/**
 * toString() override. Use Array.prototype.toString().
 */

ArrayIndex.prototype.toString = function toString () {
  var a = this.toArray();
  return a.toString.apply(a, arguments);
};

/**
 * inspect() override. For the REPL.
 */

ArrayIndex.prototype.inspect = function inspect () {
  var a = this.toArray();
  Object.keys(this).forEach(function (k) {
    a[k] = this[k];
  }, this);
  return a;
};

/**
 * Getter for the "length" property.
 * Returns the value of the "length" Symbol.
 */

function getLength () {
  debug('getting "length": %o', this[length]);
  return this[length];
};

/**
 * Setter for the "length" property.
 * Calls "ensureLength()", then sets the "length" Symbol.
 */

function setLength (v) {
  debug('setting "length": %o', v);
  return this[length] = ensureLength(this, v);
};

/**
 * Ensures that getters/setters from 0 up to "_newLength" have been defined
 * on `Object.getPrototypeOf(this)`.
 *
 * @api private
 */

function ensureLength (self, _newLength) {
  var newLength;
  if (_newLength > MAX_LENGTH) {
    newLength = MAX_LENGTH;
  } else {
    newLength = _newLength | 0;
  }
  var proto = Object.getPrototypeOf(self);
  var cur = proto[length] | 0;
  var num = newLength - cur;
  if (num > 0) {
    var desc = {};
    debug('creating a descriptor object with %o entries', num);
    for (var i = cur; i < newLength; i++) {
      desc[i] = setup(i);
    }
    debug('calling `Object.defineProperties()` with %o entries', num);
    Object.defineProperties(proto, desc);
    proto[length] = newLength;
  }
  return newLength;
}

/**
 * Returns a property descriptor for the given "index", with "get" and "set"
 * functions created within the closure.
 *
 * @api private
 */

function setup (index) {
  function get () {
    return this[ArrayIndex.get](index);
  }
  function set (v) {
    return this[ArrayIndex.set](index, v);
  }
  return {
    enumerable: true,
    configurable: true,
    get: get,
    set: set
  };
}
