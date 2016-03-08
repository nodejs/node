'use strict';

var forEach       = require('./for-each')
  , isPlainObject = require('./is-plain-object')
  , value         = require('./valid-value')

  , isArray = Array.isArray
  , copy, copyItem;

copyItem = function (value, key) {
	var index;
	if (!isPlainObject(value) && !isArray(value)) return value;
	index = this[0].indexOf(value);
	if (index === -1) return copy.call(this, value);
	return this[1][index];
};

copy = function (source) {
	var target = isArray(source) ? [] : {};
	this[0].push(source);
	this[1].push(target);
	if (isArray(source)) {
		source.forEach(function (value, key) {
			target[key] = copyItem.call(this, value, key);
		}, this);
	} else {
		forEach(source, function (value, key) {
			target[key] = copyItem.call(this, value, key);
		}, this);
	}
	return target;
};

module.exports = function (source) {
	var obj = Object(value(source));
	if (obj !== source) return obj;
	return copy.call([[], []], obj);
};
