'use strict';

var iteratorSymbol = require('es6-symbol').iterator
  , isArrayLike    = require('../object/is-array-like');

module.exports = function (x) {
	if (x == null) return false;
	if (typeof x[iteratorSymbol] === 'function') return true;
	return isArrayLike(x);
};
