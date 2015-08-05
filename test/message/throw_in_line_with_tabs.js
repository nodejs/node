/* eslint-disable indent */
'use strict';
const common = require('../common');
const assert = require('assert');

console.error('before');

(function() {
	// these lines should contain tab!
	throw ({ foo: 'bar' });
})();

console.error('after');
