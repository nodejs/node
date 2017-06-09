'use strict';

var ensure = require('./ensure-natural-number');

module.exports = function (arg) {
	if (arg == null) throw new TypeError(arg + " is not a natural number");
	return ensure(arg);
};
