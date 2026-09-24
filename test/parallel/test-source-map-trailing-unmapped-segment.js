'use strict';
require('../common');
const assert = require('assert');
const { SourceMap } = require('node:module');

// A segment with only a generated column marks the code after it as unmapped.
// When it is the last segment of the mappings, it must not inherit the
// original position of the segment before it.
const unmapped = {
  originalSource: undefined,
  originalLine: undefined,
  originalColumn: undefined,
  name: undefined,
};

// Trailing unmapped segment on the same line as the last mapped segment.
{
  const sm = new SourceMap({
    version: 3,
    sources: ['a.js'],
    names: [],
    mappings: 'AAAA,K',
  });

  assert.deepStrictEqual(sm.findEntry(0, 5), {
    generatedLine: 0,
    generatedColumn: 5,
    ...unmapped,
  });
  assert.deepStrictEqual(sm.findOrigin(1, 6), {});
}

// Trailing unmapped segment on a later line.
{
  const sm = new SourceMap({
    version: 3,
    sources: ['a.js'],
    names: [],
    mappings: 'AAAA;A',
  });

  assert.deepStrictEqual(sm.findEntry(1, 3), {
    generatedLine: 1,
    generatedColumn: 0,
    ...unmapped,
  });
  assert.deepStrictEqual(sm.findOrigin(2, 4), {});
}
