'use strict';

var compact  = require('../array/#/compact')
  , isObject = require('../object/is-object')
  , toArray  = require('../object/to-array')

  , isArray = Array.isArray, stringify = JSON.stringify;

module.exports = function self(value/*, replacer, space*/) {
	var replacer = arguments[1], space = arguments[2];
	try {
		return stringify(value, replacer, space);
	} catch (e) {
		if (!isObject(value)) return;
		if (typeof value.toJSON === 'function') return;
		if (isArray(value)) {
			return '[' +
				compact.call(value.map(function (value) { return self(value, replacer, space); })) + ']';
		}
		return '{' + compact.call(toArray(value, function (value, key) {
			value = self(value, replacer, space);
			if (!value) return;
			return stringify(key) + ':' + value;
		})).join(',') + '}';
	}
};
