'use strict';

var isNatural = require('../number/is-natural');

module.exports = function (arg) {
	var num = Number(arg);
	if (!isNatural(num)) throw new TypeError(arg + " is not a natural number");
	return num;
};
