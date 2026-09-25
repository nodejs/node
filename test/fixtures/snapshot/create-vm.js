const {
  setDeserializeMainFunction,
} = require('v8').startupSnapshot;
const assert = require('assert');

setDeserializeMainFunction(() => {
  const vm = require('vm');
  const result = vm.runInNewContext('21+21');
  console.log(`value: ${result}`);
  assert.strictEqual(result, 42);
});
