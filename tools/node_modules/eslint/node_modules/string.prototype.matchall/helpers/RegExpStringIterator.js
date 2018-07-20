'use strict';

var define = require('define-properties');
var ES = require('es-abstract');
var hasSymbols = require('has-symbols')();

var hidden = require('./hidden')();

/* eslint max-params: 1 */
var RegExpStringIterator = function RegExpStringIterator(R, S, global, fullUnicode) {
	if (ES.Type(S) !== 'String') {
		throw new TypeError('S must be a string');
	}
	if (!ES.IsRegExp(R)) {
		throw new TypeError('R must be a RegExp');
	}
	if (ES.Type(global) !== 'Boolean') {
		throw new TypeError('global must be a boolean');
	}
	if (ES.Type(fullUnicode) !== 'Boolean') {
		throw new TypeError('fullUnicode must be a boolean');
	}
	hidden.set(this, '[[IteratingRegExp]]', R);
	hidden.set(this, '[[IteratedString]]', S);
	hidden.set(this, '[[Global]]', global);
	hidden.set(this, '[[FullUnicode]]', fullUnicode);
	hidden.set(this, '[[Done]]', false);
};

define(RegExpStringIterator.prototype, {
	/* eslint complexity: 1, max-statements: 1 */
	next: function next() {
		var O = this;
		if (ES.Type(O) !== 'Object') {
			throw new TypeError('receiver must be an object');
		}
		if (!(this instanceof RegExpStringIterator) || !hidden.has(O, '[[IteratingRegExp]]') || !hidden.has(O, '[[IteratedString]]')) {
			throw new TypeError('"this" value must be a RegExpStringIterator instance');
		}
		if (hidden.get(this, '[[Done]]')) {
			return ES.CreateIterResultObject(null, true);
		}
		var R = hidden.get(this, '[[IteratingRegExp]]');
		var S = hidden.get(this, '[[IteratedString]]');
		var global = hidden.get(this, '[[Global]]');
		var fullUnicode = hidden.get(this, '[[FullUnicode]]');

		var match = ES.RegExpExec(R, S);
		if (match === null) {
			hidden.set(this, '[[Done]]', true);
			return ES.CreateIterResultObject(null, true);
		}

		if (global) {
			var matchStr = ES.ToString(ES.Get(match, '0'));
			if (matchStr === '') {
				var thisIndex = ES.ToLength(ES.Get(R, 'lastIndex'));
				var nextIndex = ES.AdvanceStringIndex(S, thisIndex, fullUnicode);
				ES.Set(R, 'lastIndex', nextIndex, true);
			}
			return ES.CreateIterResultObject(match, false);
		}
		hidden.set(this, '[[Done]]', true);
		return ES.CreateIterResultObject(match, false);
	}
});
if (hasSymbols && Symbol.toStringTag) {
	RegExpStringIterator.prototype[Symbol.toStringTag] = 'RegExp String Iterator';
	RegExpStringIterator.prototype[Symbol.iterator] = function () { return this; };
}

module.exports = RegExpStringIterator;
