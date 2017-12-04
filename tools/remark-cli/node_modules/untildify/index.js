'use strict';
var osHomedir = require('os-homedir');
var home = osHomedir();

module.exports = function (str) {
	if (typeof str !== 'string') {
		throw new TypeError('Expected a string');
	}

	return home ? str.replace(/^~($|\/|\\)/, home + '$1') : str;
};
