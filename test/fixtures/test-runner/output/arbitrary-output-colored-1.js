'use strict';

const test = require('node:test');
console.log({ foo: 'bar' });
test('passing test', () => {
  console.log(1);
});
