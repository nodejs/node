const assert = require('assert');

process.emitWarning('This warning should not be shown in the output', 'TestWarning');

console.log('process.argv:', JSON.stringify(process.argv));

assert.deepStrictEqual(process.argv.slice(2), [
  'user-arg1',
  'user-arg2',
  'user-arg3'
]);

console.log('multiple argv test passed');
