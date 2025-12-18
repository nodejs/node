'use strict';
const test = require('node:test');

console.error('stderr', 1);

test('a test', async () => {
  console.error('stderr', 2);
  await new Promise((resolve) => {
    console.log('stdout', 3);
    setTimeout(() => {
      // This should not be sent to the TAP parser.
      console.error('not ok 1 - fake test');
      resolve();
      console.log('stdout', 4);
    }, 2);
  });
  console.error('stderr', 5);
});

console.error('stderr', 6);
