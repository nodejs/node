// Flags: --expose-internals
'use strict';

require('../common');

const assert = require('assert');
const { parseHistoryFromFile } = require('internal/repl/history-utils');

function reverseString(str) {
  return str.split('\n').reverse().join('\n');
}

{
  // It properly loads a single command
  const historyFile = "console.log('lello')";
  const history = parseHistoryFromFile(historyFile);

  assert.deepStrictEqual(history, ['console.log(\'lello\')']);
}

{
  // It properly loads a single command on a multiline
  const historyFile = reverseString("console.log(\n'lello'\n)");
  const history = parseHistoryFromFile(historyFile);

  assert.deepStrictEqual(history, ['console.log(\n\'lello\'\n)']);
}

for (const declarator of ['const', 'let', 'var']) {
  {
    // It properly loads a multiline string
    const historyFile = reverseString(`${declarator} a = \`
I am a multiline string
With some very interesting content\``);
    const history = parseHistoryFromFile(historyFile);

    assert.deepStrictEqual(history, [
      `${declarator} a = \`\nI am a multiline string\nWith some very interesting content\``,
    ]);
  }

  {
    // It properly loads a multiline string with fat arrow and plus operator endings
    const historyFile = reverseString(`${declarator} a = \`
I am a multiline string +
With some very interesting content =>
With special chars here and there\``);
    const history = parseHistoryFromFile(historyFile);

    assert.deepStrictEqual(history, [
      `${declarator} a = \`\n` +
      'I am a multiline string +\n' +
      'With some very interesting content =>\n' +
      'With special chars here and there`',
    ]);
  }

  {
    // It properly loads a multiline string concatenated with the plus operator
    const historyFile = reverseString(`${declarator} a = "hello" +
'world'`);
    const history = parseHistoryFromFile(historyFile);

    assert.deepStrictEqual(history, [
      `${declarator} a = "hello" +\n'world'`,
    ]);
  }

  {
    // It properly loads a multiline object
    const historyFile = reverseString(`
${declarator} a = {
a: 1,
b: 2,
c: 3
}`);
    const history = parseHistoryFromFile(historyFile);

    assert.deepStrictEqual(history, [ `${declarator} a = {\na: 1,\nb: 2,\nc: 3\n}` ]);
  }

  {
    // It properly loads a multiline stringified object
    const historyFile = reverseString(`\`${declarator} a ={
    a: 1,
    b: 2,
    c: 3,
    d: [1, 2, 3],
  }\``);
    const history = parseHistoryFromFile(historyFile);

    assert.deepStrictEqual(history, [ `\`${declarator} a ={\n    a: 1,\n    b: 2,\n    c: 3,\n    d: [1, 2, 3],\n  }\`` ]);
  }

  {
    // It properly loads a multiline stringified array
    const historyFile = reverseString(`\`${declarator} b = [
    1,
    2,
    3,
    4,
  ]\``);
    const history = parseHistoryFromFile(historyFile);

    assert.deepStrictEqual(history, [ `\`${declarator} b = [\n    1,\n    2,\n    3,\n    4,\n  ]\`` ]);
  }

  {
    // It properly loads a multiline array
    const historyFile = reverseString(`${declarator} a = [
    1,
    2,
    3,
  ]`);
    const history = parseHistoryFromFile(historyFile);

    assert.deepStrictEqual(history, [ `${declarator} a = [\n    1,\n    2,\n    3,\n  ]` ]);
  }

  {
    // It properly loads a multiline complex array of objects
    const historyFile = reverseString(`\`${declarator} c = [
  {
    a: 1,
    b: 2,
  },
  {
    a: 3,
    b: 4,
    c: [
      1,
      2,
      3,
    ],
  }
  ]\``);

    const history = parseHistoryFromFile(historyFile);

    assert.deepStrictEqual(history, [
      `\`${declarator} c = [\n` +
      '  {\n' +
      '    a: 1,\n' +
      '    b: 2,\n' +
      '  },\n' +
      '  {\n' +
      '    a: 3,\n' +
      '    b: 4,\n' +
      '    c: [\n' +
      '      1,\n' +
      '      2,\n' +
      '      3,\n' +
      '    ],\n' +
      '  }\n' +
      '  ]`',
    ]);
  }

  {
    // It properly loads a multiline function
    const historyFile = reverseString(`function sum(a, b) {
    return a + b;
  }`);
    const history = parseHistoryFromFile(historyFile);

    assert.deepStrictEqual(history, [ 'function sum(a, b) {\n    return a + b;\n  }' ]);
  }

  {
    // It properly loads a multiline function as a fat arrow function
    const historyFile = reverseString(`${declarator} sum = (a, b) =>
    a + b;`);
    const history = parseHistoryFromFile(historyFile);

    assert.deepStrictEqual(history, [ `${declarator} sum = (a, b) =>\n    a + b;` ]);
  }

  {
    // It properly loads if statements
    const historyFile = reverseString(`if (true) {
    console.log('lello');
  }`);
    const history = parseHistoryFromFile(historyFile);

    assert.deepStrictEqual(history, [ 'if (true) {\n    console.log(\'lello\');\n  }' ]);
  }
}
