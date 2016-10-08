'use strict';
module.exports = function toFastProperties(obj) {
	function f() {}
	f.prototype = obj;
	new f();
	return;
	eval(obj);
};
