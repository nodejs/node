// Parts of implementation taken from es6-shim project
// See: https://github.com/paulmillr/es6-shim/blob/master/es6-shim.js

'use strict';

var expm1 = require('../expm1')

  , abs = Math.abs, exp = Math.exp, e = Math.E;

module.exports = function (x) {
	if (isNaN(x)) return NaN;
	x = Number(x);
	if (x === 0) return x;
	if (!isFinite(x)) return x;
	if (abs(x) < 1) return (expm1(x) - expm1(-x)) / 2;
	return (exp(x - 1) - exp(-x - 1)) * e / 2;
};
