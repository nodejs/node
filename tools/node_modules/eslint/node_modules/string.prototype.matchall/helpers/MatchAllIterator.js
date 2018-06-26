'use strict';

var ES = require('es-abstract');
var flagsGetter = require('regexp.prototype.flags');

var RegExpStringIterator = require('./RegExpStringIterator');
var OrigRegExp = RegExp;
var hasFlags = typeof (/a/).flags === 'string';

module.exports = function MatchAllIterator(R, O) {
	if (!ES.IsRegExp(R)) {
		throw new TypeError('MatchAllIterator requires a regex');
	}
	var S = ES.ToString(O);
	var C = ES.SpeciesConstructor(R, OrigRegExp);
	var flags = ES.Get(R, 'flags');

	var matcher;
	var actualFlags = typeof flags === 'string' ? flags : flagsGetter(R);
	if (hasFlags) {
		matcher = new C(R, actualFlags); // ES.Construct(C, [R, actualFlags]);
	} else if (C === OrigRegExp) {
		// workaround for older engines that lack RegExp.prototype.flags
		matcher = new C(R.source, actualFlags); // ES.Construct(C, [R.source, actualFlags]);
	} else {
		matcher = new C(R, actualFlags); // ES.Construct(C, [R, actualFlags]);
	}
	var global = ES.ToBoolean(ES.Get(R, 'global'));
	var fullUnicode = ES.ToBoolean(ES.Get(R, 'unicode'));
	var lastIndex = ES.ToLength(ES.Get(R, 'lastIndex'));
	ES.Set(matcher, 'lastIndex', lastIndex, true);
	return new RegExpStringIterator(matcher, S, global, fullUnicode);
};
