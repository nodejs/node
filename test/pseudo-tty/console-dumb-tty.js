'use strict';
require('../common');

process.env.TERM = 'dumb';

console.log({ foo: 'bar' });
console.dir({ foo: 'bar' });
console.log('%s q', 'string');
console.log('%o with object format param', { foo: 'bar' });
