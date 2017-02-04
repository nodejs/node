/* eslint-disable indent, no-tabs */
'use strict';
require('../common');

console.error('before');

(function() {
	// these lines should contain tab!
	// eslint-disable-next-line no-throw-literal
	throw ({ foo: 'bar' });
})();

console.error('after');
