'use strict';

require('../common');

function test() {
	let a = 'abc ';
	let b = 1;
	let c = ' def';
	let msg = a + b.value + c; //b.value is undefined value
	console.error(msg);
	throw new Error(msg);
}
Object.defineProperty(test, 'name', {value: 'fun ' + undefined + ' tion'});

test();
