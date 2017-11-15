'use strict';

/* Dependencies. */
var object = require('x-is-object');

/* Expose. */
module.exports = factory;

/* Methods. */
var noop = Function.prototype;
var own = {}.hasOwnProperty;

/* Handle values based on a property. */
function factory(key, options) {
  var settings = options || {};

  function one(value) {
    var fn = one.invalid;
    var handlers = one.handlers;

    if (object(value) && own.call(value, key)) {
      fn = own.call(handlers, value[key]) ? handlers[value[key]] : one.unknown;
    }

    return (fn || noop).apply(this, arguments);
  }

  one.handlers = settings.handlers || {};
  one.invalid = settings.invalid;
  one.unknown = settings.unknown;

  return one;
}
