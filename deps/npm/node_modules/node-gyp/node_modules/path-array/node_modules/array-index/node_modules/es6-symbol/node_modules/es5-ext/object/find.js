'use strict';

var findKey = require('./find-key');

module.exports = function (obj, cb/*, thisArg, compareFn*/) {
	var key = findKey.apply(this, arguments);
	return (key == null) ? key : obj[key];
};
