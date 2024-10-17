// Flags: --expose-internals
'use strict';

require('../common');
const ArrayStream = require('../common/arraystream');
const repl = require('repl');
const highlight = require('internal/repl/highlight');
const assert = require('assert');
const { stripVTControlCharacters, inspect, styleText } = require('util');

const tests = [
  // Comments (// ... and /* ... */)
  styleText('gray', '// this is a comment'),
  styleText('gray', '/* this is a inline comment */'),

  // Strings ('...', "...", and `...`)
  styleText(inspect.styles.string, '\'this is a string\''),
  styleText(inspect.styles.string, '"this is a string"'),
  /* eslint-disable-next-line no-template-curly-in-string */
  styleText(inspect.styles.string, '`this is a ${template} string`'),

  // Keywords
  styleText(inspect.styles.special, 'async'),

  // RegExp
  styleText(inspect.styles.regexp, '/regexp/g'),

  // Numbers
  styleText(inspect.styles.number, '123'),
  styleText(inspect.styles.number, '123.456'),
  styleText(inspect.styles.number, '.456'),
  styleText(inspect.styles.number, '0x123'),
  styleText(inspect.styles.number, '0xFF'),
  styleText(inspect.styles.number, '0b101'),
  styleText(inspect.styles.number, '0o123'),
  styleText(inspect.styles.number, '123e456'),
  styleText(inspect.styles.number, '123e-456'),
  styleText(inspect.styles.number, '123.'),
  styleText(inspect.styles.number, '1_2_3'),
  styleText(inspect.styles.number, 'NaN'),

  // Undefined
  styleText(inspect.styles.undefined, 'undefined'),

  // Null
  styleText(inspect.styles.null, 'null'),

  // Boolean (true/false)
  styleText(inspect.styles.boolean, 'true'),
  styleText(inspect.styles.boolean, 'false'),

  // Operators
  styleText(inspect.styles.special, '+'),
  styleText(inspect.styles.special, '-'),
  styleText(inspect.styles.special, '*'),
  styleText(inspect.styles.special, '/'),
  styleText(inspect.styles.special, '%'),
  styleText(inspect.styles.special, '**'),
  styleText(inspect.styles.special, '='),
  styleText(inspect.styles.special, '+='),
  styleText(inspect.styles.special, '-='),
  styleText(inspect.styles.special, '*='),
  styleText(inspect.styles.special, '/='),
  styleText(inspect.styles.special, '%='),
  styleText(inspect.styles.special, '**='),
  styleText(inspect.styles.special, '=='),
  styleText(inspect.styles.special, '==='),
  styleText(inspect.styles.special, '!='),
  styleText(inspect.styles.special, '!=='),
  styleText(inspect.styles.special, '>'),
  styleText(inspect.styles.special, '>='),
  styleText(inspect.styles.special, '<'),
  styleText(inspect.styles.special, '<='),
  styleText(inspect.styles.special, '>>'),
  styleText(inspect.styles.special, '<<'),
  styleText(inspect.styles.special, '>>>'),
  styleText(inspect.styles.special, '&'),
  styleText(inspect.styles.special, '|'),
  styleText(inspect.styles.special, '^'),
  styleText(inspect.styles.special, '!'),
  styleText(inspect.styles.special, '~'),
  styleText(inspect.styles.special, '&&'),
  styleText(inspect.styles.special, '||'),
  styleText(inspect.styles.special, '?'),
  styleText(inspect.styles.special, ':'),
  styleText(inspect.styles.special, '??'),
  styleText(inspect.styles.special, '??='),
  styleText(inspect.styles.special, '=>'),

  // Identifiers
  'Identifier',

  // Function Declaration
  `${styleText(inspect.styles.special, 'function')} ${styleText(inspect.styles.special, 'foo')}()`,
];

const stream = new ArrayStream();
repl.start({
  input: stream,
  output: stream,
  useGlobal: false,
  terminal: true,
  useColors: true
});

let output = '';
stream.write = (chunk) => output += chunk;

stream.run(tests);

for (const test of tests) {
  const highlighted = highlight(stripVTControlCharacters(test));
  assert.strictEqual(highlighted, test);
  assert(output.includes(highlighted));
}
