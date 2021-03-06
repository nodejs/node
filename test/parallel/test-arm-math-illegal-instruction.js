'use strict';
require('../common');

// This test ensures Math functions don't fail with an "illegal instruction"
// error on ARM devices (primarily on the Raspberry Pi 1)
// See https://github.com/nodejs/node/issues/1376
// and https://code.google.com/p/v8/issues/detail?id=4019

Math.abs(-0.5);
Math.acos(-0.5);
Math.acosh(-0.5);
Math.asin(-0.5);
Math.asinh(-0.5);
Math.atan(-0.5);
Math.atanh(-0.5);
Math.cbrt(-0.5);
Math.ceil(-0.5);
Math.cos(-0.5);
Math.cosh(-0.5);
Math.exp(-0.5);
Math.expm1(-0.5);
Math.floor(-0.5);
Math.fround(-0.5);
Math.log(-0.5);
Math.log10(-0.5);
Math.log1p(-0.5);
Math.log2(-0.5);
Math.round(-0.5);
Math.sign(-0.5);
Math.sin(-0.5);
Math.sinh(-0.5);
Math.sqrt(-0.5);
Math.tan(-0.5);
Math.tanh(-0.5);
Math.trunc(-0.5);
