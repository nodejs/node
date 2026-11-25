// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('node:assert');
const { SourceMap } = require('internal/source_map/source_map');
const { getSourceLine } = require('internal/source_map/source_map_cache');

// LINE SEPARATOR (U+2028) and PARAGRAPH SEPARATOR (U+2029) must be recognized as line terminators
const source = 'first\u2028second\u2029third\nfourth';

const sourceMap = new SourceMap({
  version: 3,
  sources: ['file:///blep.js'],
  sourcesContent: [source],
  names: [],
  mappings: '',
});

const sourcePath = sourceMap.payload.sources[0];

assert.strictEqual(getSourceLine(sourceMap, sourcePath, 0), 'first');
assert.strictEqual(getSourceLine(sourceMap, sourcePath, 1), 'second');
assert.strictEqual(getSourceLine(sourceMap, sourcePath, 2), 'third');
assert.strictEqual(getSourceLine(sourceMap, sourcePath, 3), 'fourth');
