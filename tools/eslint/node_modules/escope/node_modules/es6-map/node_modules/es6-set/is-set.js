'use strict';

var toString          = Object.prototype.toString
  , toStringTagSymbol = require('es6-symbol').toStringTag

  , id = '[object Set]'
  , Global = (typeof Set === 'undefined') ? null : Set;

module.exports = function (x) {
	return (x && ((Global && (x instanceof Global)) ||
			(toString.call(x) === id) || (x[toStringTagSymbol] === 'Set'))) || false;
};
