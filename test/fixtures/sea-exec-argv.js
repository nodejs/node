const assert = require('assert');

process.emitWarning('This warning should not be shown in the output', 'TestWarning');

console.log('process.argv:', JSON.stringify(process.argv));
console.log('process.execArgv:', JSON.stringify(process.execArgv));

assert.deepStrictEqual(process.execArgv, [ '--no-warnings', '--max-old-space-size=2048' ]);

// We start from 2, because in SEA, the index 1 would be the same as the execPath
// to accommodate the general expectation that index 1 is the path to script for
// applications.
assert.deepStrictEqual(process.argv.slice(2), [
  'user-arg1',
  'user-arg2'
]);

console.log('multiple execArgv test passed');
