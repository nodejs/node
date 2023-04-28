'use strict';

const common = require('../common');
const ArrayStream = require('../common/arraystream');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const repl = require('repl');
let found = false;

process.on('exit', () => {
  assert.strictEqual(found, true);
});

ArrayStream.prototype.write = function(output) {
  // Matching only on a minimal piece of the stack because the string will vary
  // greatly depending on the JavaScript engine. V8 includes `;` because it
  // displays the line of code (`var foo bar;`) that is causing a problem.
  // ChakraCore does not display the line of code but includes `;` in the phrase
  // `Expected ';' `.
  if (/;/.test(output))
    found = true;
};

const putIn = new ArrayStream();
repl.start('', putIn);
let file = fixtures.path('syntax', 'bad_syntax');

if (common.isWindows)
  file = file.replace(/\\/g, '\\\\');

putIn.run(['.clear']);
putIn.run([`require('${file}');`]);
