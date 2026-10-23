'use strict';
require('../common');
const assert = require('assert');
const { SourceMap } = require('node:module');

// Section offsets are stored in an Int32Array, so they must be validated
// instead of being implicitly coerced.
function sectionedMap(offset) {
  return {
    version: 3,
    sections: [{
      offset,
      map: {
        version: 3,
        sources: ['a.js'],
        names: [],
        mappings: 'AAAA',
      },
    }],
  };
}

{
  const sm = new SourceMap(sectionedMap({ line: 1, column: 2 }));
  assert.deepStrictEqual(sm.findEntry(1, 2), {
    generatedLine: 1,
    generatedColumn: 2,
    originalSource: 'a.js',
    originalLine: 0,
    originalColumn: 0,
    name: undefined,
  });
}

for (const value of ['1', 1.5, 2 ** 31, null, {}]) {
  assert.throws(() => new SourceMap(sectionedMap({ line: value, column: 0 })), {
    code: /^ERR_(INVALID_ARG_TYPE|OUT_OF_RANGE)$/,
  });
  assert.throws(() => new SourceMap(sectionedMap({ line: 0, column: value })), {
    code: /^ERR_(INVALID_ARG_TYPE|OUT_OF_RANGE)$/,
  });
}
