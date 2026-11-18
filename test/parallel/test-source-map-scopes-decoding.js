// Flags: --expose-internals
'use strict';

// Decoding of the scopes proposal beyond the test426 decoding tests in
// test/test426/test-source-map-decoding.mjs.

require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const { SourceMap, kScopes } = require('internal/source_map/source_map');

// DecodeIndexSourceMap appends the sources of each section, with their
// [[RootScopes]], and has no [[Ranges]].
{
  const section = JSON.parse(
    fixtures.readSync(['test426', 'decoding', 'scopes', 'range-call-site.map'], 'utf8'));
  const { originalScopes, generatedRanges } = new SourceMap({
    version: 3,
    sections: [
      { offset: { line: 0, column: 0 }, map: section },
      { offset: { line: 20, column: 4 }, map: section },
    ],
  })[kScopes];
  assert.strictEqual(originalScopes.length, 2);
  assert.strictEqual(originalScopes[0][0].name, null);
  assert.strictEqual(originalScopes[1][0].children[0].name, 'inlineMe');
  assert.strictEqual(originalScopes[1][0].sourceIndex, 1);
  assert.deepStrictEqual(generatedRanges, []);
}

// A range can reference the innermost of deeply nested scopes.
{
  const depth = 50_000;
  const { originalScopes, generatedRanges } = new SourceMap({
    version: 3,
    sources: ['original.js'],
    names: [],
    mappings: '',
    scopes: ['BAAA,'.repeat(depth) + 'CAA,'.repeat(depth - 1) + 'CAA'],
    // Definition: source 0, scope index 49999 (`+0hD`), the innermost scope.
    ranges: ['ECAA+0hD,FA'],
  })[kScopes];
  let innermost = originalScopes[0][0];
  for (let i = 1; i < depth; ++i) innermost = innermost.children[0];
  assert.strictEqual(innermost.children.length, 0);
  assert.strictEqual(generatedRanges[0].definition, innermost);
}

// Malformed scopes and ranges decode to nothing instead of throwing.
for (const [scopes, ranges, scopeCount, rangeCount] of [
  ['BCAAA,CKA', 'ECAAA,FKA', 1, 1], // Well-formed.
  ['BCAAA', 'ECAAA', 0, 0], // Start without an end.
  ['CKA', 'FKA', 0, 0], // End without a start.
  ['BCAA!,CKA', 'ECAA!,FKA', 0, 0], // Invalid base64 digit.
  ['BCAAg,CKA', 'ECAAg,FKA', 0, 0], // Truncated VLQ.
  ['BCAAA,ZZ,CKA', 'ECAAA,ZZ,FKA', 1, 1], // Unknown items are ignored.
  ['BA//////DA,CAA', 'EA//////D,FA', 1, 1], // A Vlq of 2^32 - 1.
  ['BAggggggEA,CAA', 'EAggggggE,FA', 0, 0], // A Vlq of 2^32.
]) {
  const { originalScopes, generatedRanges } = new SourceMap({
    version: 3,
    sources: ['original.js'],
    names: ['global'],
    mappings: '',
    scopes: [scopes],
    ranges: [ranges],
  })[kScopes];
  assert.strictEqual(originalScopes[0].length, scopeCount, scopes);
  assert.strictEqual(generatedRanges.length, rangeCount, ranges);
}
