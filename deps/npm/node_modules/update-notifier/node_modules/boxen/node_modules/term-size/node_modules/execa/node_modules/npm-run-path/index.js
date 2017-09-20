'use strict';
var path = require('path');
var pathKey = require('path-key');

module.exports = function (opts) {
	opts = opts || {};

	var prev;
	var pth = path.resolve(opts.cwd || '.');

	var ret = [];

	while (prev !== pth) {
		ret.push(path.join(pth, 'node_modules/.bin'));
		prev = pth;
		pth = path.resolve(pth, '..');
	}

	// ensure the running `node` binary is used
	ret.push(path.dirname(process.execPath));

	return ret.concat(opts.path || process.env[pathKey()]).join(path.delimiter);
};
