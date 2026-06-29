'use strict';

// Support `exports` as an alternative to `module.exports`.

function Buffer() {};

exports.Buffer = Buffer;
exports.fn1 = function fn1() {};

var fn2 = exports.fn2 = function() {};

function fn3() {};
exports.fn3 = fn3;
