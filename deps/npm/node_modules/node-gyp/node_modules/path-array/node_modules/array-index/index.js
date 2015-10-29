
/**
 * Module dependencies.
 */

var util = require('util')
var debug = require('debug')('array-index')

/**
 * JavaScript Array "length" is bound to an unsigned 32-bit int.
 * See: http://stackoverflow.com/a/6155063/376773
 */

var MAX_LENGTH = Math.pow(2, 32)

/**
 * Module exports.
 */

module.exports = ArrayIndex

/**
 * Subclass this.
 */

function ArrayIndex (length) {
  Object.defineProperty(this, 'length', {
    get: getLength,
    set: setLength,
    enumerable: false,
    configurable: true
  })

  Object.defineProperty(this, '__length', {
    value: 0,
    writable: true,
    enumerable: false,
    configurable: true
  })

  if (arguments.length > 0) {
    this.length = length
  }
}

/**
 * You overwrite the "__get__" function in your subclass.
 */

ArrayIndex.prototype.__get__ = function () {
  throw new Error('you must implement the __get__ function')
}

/**
 * You overwrite the "__set__" function in your subclass.
 */

ArrayIndex.prototype.__set__ = function () {
  throw new Error('you must implement the __set__ function')
}

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
  var i = 0, l = this.length, array = new Array(l)
  for (; i < l; i++) {
    array[i] = this[i]
  }
  return array
}

/**
 * Basic support for `JSON.stringify()`.
 */

ArrayIndex.prototype.toJSON = function toJSON () {
  return this.toArray()
}

/**
 * toString() override. Use Array.prototype.toString().
 */

ArrayIndex.prototype.toString = function toString () {
  var a = this.toArray()
  return a.toString.apply(a, arguments)
}

/**
 * inspect() override. For the REPL.
 */

ArrayIndex.prototype.inspect = function inspect () {
  var a = this.toArray()
  Object.keys(this).forEach(function (k) {
    a[k] = this[k]
  }, this)
  return util.inspect(a)
}

/**
 * Getter for the "length" property.
 * Returns the value of the "__length" property.
 */

function getLength () {
  debug('getting "length": %o', this.__length)
  return this.__length
}

/**
 * Setter for the "length" property.
 * Calls "ensureLength()", then sets the "__length" property.
 */

function setLength (v) {
  debug('setting "length": %o', v)
  return this.__length = ensureLength(v)
}

/**
 * Ensures that getters/setters from 0 up to "_length" have been defined
 * on `ArrayIndex.prototype`.
 *
 * @api private
 */

function ensureLength (_length) {
  var length
  if (_length > MAX_LENGTH) {
    length = MAX_LENGTH
  } else {
    length = _length | 0
  }
  var cur = ArrayIndex.prototype.__length__ | 0
  var num = length - cur
  if (num > 0) {
    var desc = {}
    debug('creating a descriptor object with %o entries', num)
    for (var i = cur; i < length; i++) {
      desc[i] = setup(i)
    }
    debug('done creating descriptor object')
    debug('calling `Object.defineProperties()` with %o entries', num)
    Object.defineProperties(ArrayIndex.prototype, desc)
    debug('finished `Object.defineProperties()`')
    ArrayIndex.prototype.__length__ = length
  }
  return length
}

/**
 * Returns a property descriptor for the given "index", with "get" and "set"
 * functions created within the closure.
 *
 * @api private
 */

function setup (index) {
  function get () {
    return this.__get__(index)
  }
  function set (v) {
    return this.__set__(index, v)
  }
  return {
      enumerable: true
    , configurable: true
    , get: get
    , set: set
  }
}
