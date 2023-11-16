'use strict';

var call = Function.prototype.call;
var $hasOwn = Object.prototype.hasOwnProperty;
var bind = require('function-bind');

/** @type {(o: {}, p: PropertyKey) => p is keyof o} */
module.exports = bind.call(call, $hasOwn);
