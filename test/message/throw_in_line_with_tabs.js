/* eslint-disable indent */
'use strict';
var common = require('../common');
var assert = require('assert');

console.error('before');

(function() {
	// these lines should contain tab!
	throw ({ foo: 'bar' });
})();

console.error('after');
