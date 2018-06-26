'use strict';

var ES = require('es-abstract');
var hasSymbols = require('has-symbols')();

var OrigRegExp = RegExp;

var MatchAllIterator = require('./helpers/MatchAllIterator');

module.exports = function matchAll(regexp) {
	var O = ES.RequireObjectCoercible(this);
	var R;
	if (ES.IsRegExp(regexp)) {
		R = regexp;
	} else {
		R = new OrigRegExp(regexp, 'g');
	}
	var matcher;
	if (hasSymbols && typeof Symbol.matchAll === 'symbol') {
		matcher = ES.GetMethod(R, Symbol.matchAll);
	}
	if (typeof matcher !== 'undefined') {
		return ES.Call(matcher, R, [O]);
	}

	return MatchAllIterator(R, O);
};
