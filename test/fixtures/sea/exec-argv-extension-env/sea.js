const assert = require('assert');

process.emitWarning('This warning should not be shown in the output', 'TestWarning');

console.log('process.argv:', JSON.stringify(process.argv));
console.log('process.execArgv:', JSON.stringify(process.execArgv));

// Should have execArgv from SEA config.
// Note that flags from NODE_OPTIONS are not included in process.execArgv no matter it's
// an SEA or not, but we can test whether it works by checking that the warning emitted
// above was silenced.
assert.deepStrictEqual(process.execArgv, ['--no-warnings']);

assert.deepStrictEqual(process.argv.slice(2), [
  'user-arg1', 
  'user-arg2'
]);

console.log('execArgvExtension env test passed');
