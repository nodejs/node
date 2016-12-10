'use strict';

module.exports = function (stringifiable) {
	try {
		return String(stringifiable);
	} catch (e) {
		throw new TypeError("Passed argument cannot be stringifed");
	}
};
