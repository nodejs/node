// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const { inspect, stripVTControlCharacters } = require('util');
const ArrayStream = require('../common/arraystream');
const { highlight } = require('internal/repl/highlight');
const { REPLServer } = require('repl');

assert.strictEqual(stripVTControlCharacters(highlight(
  'const /* comment */ answer = 42; // comment',
)), 'const /* comment */ answer = 42; // comment');

assert.strictEqual(
  highlight('const value = "text"'),
  `\u001b[${inspect.colors.cyan[0]}mconst\u001b[${inspect.colors.cyan[1]}m value = ` +
  `\u001b[${inspect.colors.green[0]}m"text"\u001b[${inspect.colors.green[1]}m`,
);

for (const source of [
  'undefined',
  'NaN',
  'Infinity',
  'Symbol.iterator',
  'Date.now()',
  'true',
  'null',
  '1n',
  '/regexp/u',
  'import("node:fs")',
]) {
  assert.notStrictEqual(highlight(source), source);
}

// Incomplete source is expected while the user is typing.
highlight('const value = "');

{
  const input = new ArrayStream();
  const output = new ArrayStream();
  let rendered = '';
  output.write = (chunk) => {
    rendered += chunk;
  };

  const repl = new REPLServer({
    input,
    output,
    prompt: '',
    terminal: true,
    useColors: false,
  });
  rendered = '';
  repl.write('const');

  assert.strictEqual(rendered, 'const');
  repl.close();
}
