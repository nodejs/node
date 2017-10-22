'use strict';
var crypto = require('crypto');

module.exports = function (input) {
	var hash = crypto.createHash('md5');

	var update = function (buf) {
		var inputEncoding = typeof buf === 'string' ? 'utf8' : undefined;
		hash.update(buf, inputEncoding);
	};

	if (arguments.length > 1) {
		throw new Error('Too many arguments. Try specifying an array.');
	}

	if (Array.isArray(input)) {
		input.forEach(update);
	} else {
		update(input);
	}

	return hash.digest('hex');
};
