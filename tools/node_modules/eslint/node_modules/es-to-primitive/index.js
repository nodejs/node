'use strict';

var ES5 = require('./es5');
var ES6 = require('./es6');

if (Object.defineProperty) {
	Object.defineProperty(ES6, 'ES5', { enumerable: false, value: ES5 });
	Object.defineProperty(ES6, 'ES6', { enumerable: false, value: ES6 });
} else {
	ES6.ES5 = ES5;
	ES6.ES6 = ES6;
}

module.exports = ES6;
