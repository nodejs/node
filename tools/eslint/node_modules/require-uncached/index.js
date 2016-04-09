'use strict';
var path = require('path');
var resolveFrom = require('resolve-from');
var callerPath = require('caller-path');

module.exports = function (moduleId) {
	if (typeof moduleId !== 'string') {
		throw new TypeError('Expected a string');
	}

	var filePath = resolveFrom(path.dirname(callerPath()), moduleId);
	var tmp = require.cache[filePath];
	delete require.cache[filePath];
	var ret = require(filePath);
	require.cache[filePath] = tmp;

	return ret;
};
