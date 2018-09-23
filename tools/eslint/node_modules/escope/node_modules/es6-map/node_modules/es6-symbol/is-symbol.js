'use strict';

var toString    = Object.prototype.toString
  , toStringTag = require('./').toStringTag

  , id = '[object Symbol]'
  , Global = (typeof Symbol === 'undefined') ? null : Symbol;

module.exports = function (x) {
	return (x && ((Global && (x instanceof Global)) ||
			(toString.call(x) === id) || (x[toStringTag] === 'Symbol'))) || false;
};
