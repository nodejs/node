'use strict';

require('../common');
const { strictEqual, fail } = require('assert');
const { readFileSync } = require('fs');

const { containsModuleSyntax } = require('module');

expect('esm-with-import-statement.js', 'module');
expect('esm-with-export-statement.js', 'module');
expect('esm-with-import-expression.js', 'module');
expect('esm-with-indented-import-statement.js', 'module');
expect('hashbang.js', 'module');

expect('cjs-with-require.js', 'commonjs');
expect('cjs-with-import-expression.js', 'commonjs');
expect('cjs-with-property-named-import.js', 'commonjs');
expect('cjs-with-property-named-export.js', 'commonjs');
expect('cjs-with-string-containing-import.js', 'commonjs');

expect('print-version.js', 'commonjs');
expect('ambiguous-with-import-expression.js', 'commonjs');

expect('syntax-error.js', 'Invalid or unexpected token', true);

function expect(file, want, wantsError = false) {
  const source = readFileSync(
    require.resolve(`../fixtures/es-modules/detect/${file}`),
    'utf8');
  let isModule;
  try {
    isModule = containsModuleSyntax(source);
  } catch (err) {
    if (wantsError) {
      return strictEqual(err.message, want);
    } else {
      return fail(
        `Expected ${file} to throw '${want}'; received '${err.message}'`);
    }
  }
  if (wantsError)
    return fail(`Expected ${file} to throw '${want}'; no error was thrown`);
  else
    return strictEqual((isModule ? 'module' : 'commonjs'), want);
}
