'use strict';
var md5OMatic = require('md5-o-matic');

module.exports = function (input) {
	if (Array.isArray(input)) {
		input = input.join('');
	}

	return md5OMatic(input);
};
