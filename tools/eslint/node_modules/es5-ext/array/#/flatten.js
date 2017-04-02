// Stack grow safe implementation

'use strict';

var ensureValue = require('../../object/valid-value')

  , isArray = Array.isArray, hasOwnProperty = Object.prototype.hasOwnProperty;

module.exports = function () {
	var input = ensureValue(this), index = 0, remaining, remainingIndexes, l, i, result = [];
	main: //jslint: ignore
	while (input) {
		l = input.length;
		for (i = index; i < l; ++i) {
			if (!hasOwnProperty.call(input, i)) continue;
			if (isArray(input[i])) {
				if (i < (l - 1)) {
					if (!remaining) {
						remaining = [];
						remainingIndexes = [];
					}
					remaining.push(input);
					remainingIndexes.push(i + 1);
				}
				input = input[i];
				index = 0;
				continue main;
			}
			result.push(input[i]);
		}
		if (remaining) {
			input = remaining.pop();
			index = remainingIndexes.pop();
		} else {
			input = null;
		}
	}
	return result;
};
