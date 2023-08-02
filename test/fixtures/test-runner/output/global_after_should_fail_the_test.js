'use strict';
const { it, after } = require('node:test');

after(() => {
  throw new Error('this should fail the test')
});

it('this is a test', () => {
  console.log('this is a test')
});
