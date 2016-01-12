'use strict';
var escapeStringRegexp = require('escape-string-regexp');

module.exports = function (str, sep) {
	if (typeof str !== 'string') {
		throw new TypeError('Expected a string');
	}

	sep = typeof sep === 'undefined' ? '_' : sep;

	var reSep = escapeStringRegexp(sep);

	return str.replace(/([a-z\d])([A-Z])/g, '$1' + sep + '$2')
					.replace(new RegExp('(' + reSep + '[A-Z])([A-Z])', 'g'), '$1' + reSep + '$2')
					.toLowerCase();
};
