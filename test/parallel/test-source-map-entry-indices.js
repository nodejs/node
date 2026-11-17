'use strict';
require('../common');
const assert = require('assert');
const { SourceMap } = require('node:module');

// Segments listed out of column order are looked up by generated position.
{
  const sm = new SourceMap({
    version: 3,
    sources: ['a.js'],
    names: [],
    mappings: 'KAAA,LAAC',
  });

  assert.strictEqual(sm.findEntry(0, 0).originalColumn, 1);
  assert.strictEqual(sm.findEntry(0, 6).originalColumn, 0);
}

// In an index map, a source or name index outside its own section's lists
// does not resolve to an entry of another section.
{
  const sm = new SourceMap({
    version: 3,
    sections: [
      {
        offset: { line: 0, column: 0 },
        map: {
          version: 3,
          sources: ['a.js'],
          names: ['first'],
          mappings: 'AAAAA,ECAAC',
        },
      },
      {
        offset: { line: 1, column: 0 },
        map: {
          version: 3,
          sources: ['b.js'],
          names: ['second'],
          mappings: 'AAAAA',
        },
      },
    ],
  });

  assert.deepStrictEqual(sm.findEntry(0, 0), {
    generatedLine: 0,
    generatedColumn: 0,
    originalSource: 'a.js',
    originalLine: 0,
    originalColumn: 0,
    name: 'first',
  });
  assert.deepStrictEqual(sm.findEntry(0, 2), {
    generatedLine: 0,
    generatedColumn: 2,
    originalSource: undefined,
    originalLine: 0,
    originalColumn: 0,
    name: undefined,
  });
  assert.deepStrictEqual(sm.findEntry(1, 0), {
    generatedLine: 1,
    generatedColumn: 0,
    originalSource: 'b.js',
    originalLine: 0,
    originalColumn: 0,
    name: 'second',
  });
}
