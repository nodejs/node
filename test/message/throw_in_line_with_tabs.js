/* eslint-disable indent-legacy, no-tabs */
'use strict';
require('../common');

console.error('before');

(function() {
	// these lines should contain tab!
	throw ({ foo: 'bar' });
})();

console.error('after');
