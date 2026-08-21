'use strict';

require('../common');
const util = require('util');

const err = new Error('foo\nbar');

console.log(util.inspect({ err, nested: { err } }, { compact: true }));
console.log(util.inspect({ err, nested: { err } }, { compact: false }));

err.foo = 'bar';
console.log(util.inspect(err, { compact: true, breakLength: 5 }));
