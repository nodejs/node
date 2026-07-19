const assert = require('assert');

console.log('process.argv:', JSON.stringify(process.argv));
assert.strictEqual(process.argv[2], 'user-arg');
assert.deepStrictEqual(process.execArgv, []);
console.log('empty execArgv test passed');
