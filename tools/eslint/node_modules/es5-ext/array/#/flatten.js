'use strict';

var isArray = Array.isArray, forEach = Array.prototype.forEach;

module.exports = function flatten() {
	var r = [];
	forEach.call(this, function (x) {
		if (isArray(x)) {
			r = r.concat(flatten.call(x));
		} else {
			r.push(x);
		}
	});
	return r;
};
