'use strict';
require('../common');
// Make this test OS-independent by overriding stdio getColorDepth().
process.stdout.getColorDepth = () => 8;
process.stderr.getColorDepth = () => 8;

console.log({ foo: 'bar' });
console.log('%s q', 'string');
console.log('%o with object format param', { foo: 'bar' });
