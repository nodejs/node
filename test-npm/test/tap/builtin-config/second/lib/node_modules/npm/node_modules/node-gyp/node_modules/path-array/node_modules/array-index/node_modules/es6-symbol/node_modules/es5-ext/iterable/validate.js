'use strict';

var is = require('./is');

module.exports = function (x) {
	if (is(x)) return x;
	throw new TypeError(x + " is not an iterable or array-like");
};
