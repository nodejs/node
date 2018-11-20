'use strict';

var d        = require('d')
  , isSymbol = require('../is-symbol')

  , defineProperty = Object.defineProperty;

module.exports = function (T, a) {
	var symbol = T('test'), x = {};
	defineProperty(x, symbol, d('foo'));
	a(x.test, undefined, "Name");
	a(x[symbol], 'foo', "Get");

	a(isSymbol(symbol), true, "Symbol");
	a(isSymbol(T.iterator), true, "iterator");
	a(isSymbol(T.toStringTag), true, "toStringTag");
};
