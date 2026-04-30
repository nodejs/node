'use strict';
const test = require('node:test');
const unparseable = require('./coverage-unparseable.js');

test('require unparseable module', () => {
  // The unparseable module loads fine at runtime (CJS wrapper makes
  // top-level `using` valid) but acorn cannot parse the raw source.
  // Statement coverage should degrade gracefully for that file.
  require('node:assert').ok(unparseable.ok);
});
