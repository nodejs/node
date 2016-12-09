'use strict';

var isDate = require('./is-date');

module.exports = function (x) {
	if (!isDate(x) || isNaN(x)) throw new TypeError(x + " is not valid Date object");
	return x;
};
