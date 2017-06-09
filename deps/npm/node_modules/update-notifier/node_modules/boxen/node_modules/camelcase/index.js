'use strict';

function preserveCamelCase(str) {
	let isLastCharLower = false;
	let isLastCharUpper = false;
	let isLastLastCharUpper = false;

	for (let i = 0; i < str.length; i++) {
		const c = str.charAt(i);

		if (isLastCharLower && (/[a-zA-Z]/).test(c) && c.toUpperCase() === c) {
			str = str.substr(0, i) + '-' + str.substr(i);
			isLastCharLower = false;
			isLastLastCharUpper = isLastCharUpper;
			isLastCharUpper = true;
			i++;
		} else if (isLastCharUpper && isLastLastCharUpper && (/[a-zA-Z]/).test(c) && c.toLowerCase() === c) {
			str = str.substr(0, i - 1) + '-' + str.substr(i - 1);
			isLastLastCharUpper = isLastCharUpper;
			isLastCharUpper = false;
			isLastCharLower = true;
		} else {
			isLastCharLower = c.toLowerCase() === c;
			isLastLastCharUpper = isLastCharUpper;
			isLastCharUpper = c.toUpperCase() === c;
		}
	}

	return str;
}

module.exports = function () {
	let str = [].map.call(arguments, x => x.trim()).filter(x => x.length).join('-');

	if (str.length === 0) {
		return '';
	}

	if (str.length === 1) {
		return str.toLowerCase();
	}

	str = preserveCamelCase(str);

	return str
		.replace(/^[_.\- ]+/, '')
		.toLowerCase()
		.replace(/[_.\- ]+(\w|$)/g, (m, p1) => p1.toUpperCase());
};
