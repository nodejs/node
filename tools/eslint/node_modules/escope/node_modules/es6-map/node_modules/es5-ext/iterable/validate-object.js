'use strict';

var isObject = require('../object/is-object')
  , is       = require('./is');

module.exports = function (x) {
	if (is(x) && isObject(x)) return x;
	throw new TypeError(x + " is not an iterable or array-like object");
};
