'use strict';

var exp = Math.exp;

module.exports = function (x) {
	var a, b;
	if (isNaN(x)) return NaN;
	x = Number(x);
	if (x === 0) return x;
	if (x === Infinity) return 1;
	if (x === -Infinity) return -1;
	a = exp(x);
	if (a === Infinity) return 1;
	b = exp(-x);
	if (b === Infinity) return -1;
	return (a - b) / (a + b);
};
