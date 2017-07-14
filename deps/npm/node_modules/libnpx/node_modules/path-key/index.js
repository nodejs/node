'use strict';
module.exports = function (opts) {
	opts = opts || {};

	var env = opts.env || process.env;
	var platform = opts.platform || process.platform;

	if (platform !== 'win32') {
		return 'PATH';
	}

	return Object.keys(env).filter(function (x) {
		return x.toUpperCase() === 'PATH';
	})[0] || 'Path';
};
