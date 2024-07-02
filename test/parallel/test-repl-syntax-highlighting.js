// Flags: --expose-internals
'use strict';

require('../common');
const highlight = require('internal/repl/highlight');
const assert = require('assert');
const { inspect } = require('internal/util/inspect');

function removeEscapeSequences(str) {
  /* eslint-disable-next-line no-control-regex */
  return str.replace(/\u001b\[\d+m/g, '');
}

function color(str, color) {
  return `\u001b[${color[0]}m${str}\u001b[${color[1]}m`;
}

const tests = [
  // Comments (// ... and /* ... */)
  color('// this is a comment', inspect.colors.gray),
  color('/* this is a\nmultiline comment */', inspect.colors.gray),

  // Strings ('...', "...", and `...`)
  color('\'this is a string\'', inspect.colors[inspect.styles.string]),
  color('"this is a string"', inspect.colors[inspect.styles.string]),
  /* eslint-disable-next-line no-template-curly-in-string */
  color('`this is a ${template}\nstring`', inspect.colors[inspect.styles.string]),

  // Keywords
  color('async', inspect.colors[inspect.styles.special]),

  // RegExp
  color('/regexp/g', inspect.colors[inspect.styles.regexp]),

  // Numbers
  color('123', inspect.colors[inspect.styles.number]),
  color('123.456', inspect.colors[inspect.styles.number]),
  color('.456', inspect.colors[inspect.styles.number]),
  color('0x123', inspect.colors[inspect.styles.number]),
  color('0xFF', inspect.colors[inspect.styles.number]),
  color('0b101', inspect.colors[inspect.styles.number]),
  color('0o123', inspect.colors[inspect.styles.number]),
  color('123e456', inspect.colors[inspect.styles.number]),
  color('123e-456', inspect.colors[inspect.styles.number]),
  color('123.', inspect.colors[inspect.styles.number]),
  color('1_2_3', inspect.colors[inspect.styles.number]),
  color('NaN', inspect.colors[inspect.styles.number]),

  // Undefined
  color('undefined', inspect.colors[inspect.styles.undefined]),

  // Null
  color('null', inspect.colors[inspect.styles.null]),

  // Boolean (true/false)
  color('true', inspect.colors[inspect.styles.boolean]),
  color('false', inspect.colors[inspect.styles.boolean]),

  // Operators
  color('+', inspect.colors[inspect.styles.special]),
  color('-', inspect.colors[inspect.styles.special]),
  color('*', inspect.colors[inspect.styles.special]),
  color('/', inspect.colors[inspect.styles.special]),
  color('%', inspect.colors[inspect.styles.special]),
  color('**', inspect.colors[inspect.styles.special]),
  color('=', inspect.colors[inspect.styles.special]),
  color('+=', inspect.colors[inspect.styles.special]),
  color('-=', inspect.colors[inspect.styles.special]),
  color('*=', inspect.colors[inspect.styles.special]),
  color('/=', inspect.colors[inspect.styles.special]),
  color('%=', inspect.colors[inspect.styles.special]),
  color('**=', inspect.colors[inspect.styles.special]),
  color('==', inspect.colors[inspect.styles.special]),
  color('===', inspect.colors[inspect.styles.special]),
  color('!=', inspect.colors[inspect.styles.special]),
  color('!==', inspect.colors[inspect.styles.special]),
  color('>', inspect.colors[inspect.styles.special]),
  color('>=', inspect.colors[inspect.styles.special]),
  color('<', inspect.colors[inspect.styles.special]),
  color('<=', inspect.colors[inspect.styles.special]),
  color('>>', inspect.colors[inspect.styles.special]),
  color('<<', inspect.colors[inspect.styles.special]),
  color('>>>', inspect.colors[inspect.styles.special]),
  color('&', inspect.colors[inspect.styles.special]),
  color('|', inspect.colors[inspect.styles.special]),
  color('^', inspect.colors[inspect.styles.special]),
  color('!', inspect.colors[inspect.styles.special]),
  color('~', inspect.colors[inspect.styles.special]),
  color('&&', inspect.colors[inspect.styles.special]),
  color('||', inspect.colors[inspect.styles.special]),
  color('?', inspect.colors[inspect.styles.special]),
  color(':', inspect.colors[inspect.styles.special]),
  color('??', inspect.colors[inspect.styles.special]),
  color('??=', inspect.colors[inspect.styles.special]),
  color('=>', inspect.colors[inspect.styles.special]),

  // Identifiers
  'Identifier',

  // Function Declaration
  `${color('function', inspect.colors[inspect.styles.special])} ${color('foo', inspect.colors[inspect.styles.special])}()`,
];

for (const test of tests) {
  const result = highlight(removeEscapeSequences(test));
  assert.strictEqual(result, test);
}
