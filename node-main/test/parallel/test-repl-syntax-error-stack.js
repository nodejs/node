'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const { startNewREPLServer } = require('../common/repl');

let found = false;

process.on('exit', () => {
  assert.strictEqual(found, true);
});

const { input, output } = startNewREPLServer({}, { disableDomainErrorAssert: true });

output.write = (data) => {
  // Matching only on a minimal piece of the stack because the string will vary
  // greatly depending on the JavaScript engine. V8 includes `;` because it
  // displays the line of code (`var foo bar;`) that is causing a problem.
  // ChakraCore does not display the line of code but includes `;` in the phrase
  // `Expected ';' `.
  if (/;/.test(data))
    found = true;
};

let file = fixtures.path('syntax', 'bad_syntax');

if (common.isWindows)
  file = file.replace(/\\/g, '\\\\');

input.run(['.clear']);
input.run([`require('${file}');`]);
