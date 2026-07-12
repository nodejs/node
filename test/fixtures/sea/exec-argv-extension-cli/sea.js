const assert = require('assert');

console.log('process.argv:', JSON.stringify(process.argv));
console.log('process.execArgv:', JSON.stringify(process.execArgv));

// Should have execArgv from SEA config + CLI --node-options
assert.deepStrictEqual(process.execArgv, ['--no-warnings', '--max-old-space-size=1024']);

assert.deepStrictEqual(process.argv.slice(2), [
  'user-arg1',
  'user-arg2'
]);

console.log('execArgvExtension cli test passed');
