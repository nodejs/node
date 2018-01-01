// Flags: --expose-internals
const assert = require('assert');
const { parseFunctionArgs } = require('internal/repl/parser');

{
  let inputs = [{
    args: `"B", "c,d", 12, "int`,
    expectedResult: ['B', 'c,d', 12, '\"int'],
  }, {
    args: `'hello', 'hi'`,
    expectedResult: ['hello', 'hi'],
  }, {
    args: '\'',
    expectedResult: [''],
  }, {
    args: '\"',
    expectedResult: [''],
  }, {
    args: '\"hello\", ',
    expectedResult: ['hello', ''],
  }, {
    args: '',
    expectedResult: [''],
  }, {
    args: '13, \'\'',
    expectedResult: [13, ''],
  }];

  inputs.forEach(input => {
    const results = parseFunctionArgs(input.args);
    assert.strictEqual(results.length, input.expectedResult.length);
    assert.deepStrictEqual(results, input.expectedResult);
  })
}
