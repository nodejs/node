const assert = require('assert');

console.log('process.argv:', JSON.stringify(process.argv));
console.log('process.execArgv:', JSON.stringify(process.execArgv));

// Should only have execArgv from SEA config, no NODE_OPTIONS
assert.deepStrictEqual(process.execArgv, ['--no-warnings']);

assert.deepStrictEqual(process.argv.slice(2), [
  'user-arg1',
  'user-arg2'
]);

console.log('execArgvExtension none test passed');
