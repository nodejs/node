// Flags: --enable-source-maps --expose-internals
'use strict';

/**
 * This test verifies that the cache of source maps of generated sources stays
 * within its budget, and that reading an entry keeps it from being evicted.
 */

require('../common');
const assert = require('node:assert');
const { findSourceMap } = require('node:module');
const {
  kGeneratedSourceMapCacheSizeLimit,
} = require('internal/source_map/source_map_cache');

const sourcesContent = 'x'.repeat(1024 * 1024);
const payload = Buffer.from(JSON.stringify({
  version: 3,
  sources: ['a.js'],
  sourcesContent: [sourcesContent],
  names: [],
  mappings: 'AAAA',
})).toString('base64');

function evaluate(id) {
  eval(`(() => {})\n` +
       `//# sourceURL=file:///generated-${id}.js\n` +
       `//# sourceMappingURL=data:application/json;base64,${payload}`);
}

// Enough generated sources to overrun the budget twice over.
const count = Math.ceil(kGeneratedSourceMapCacheSizeLimit / payload.length) * 2;

evaluate(0);
evaluate(1);
assert.ok(findSourceMap('file:///generated-0.js'));

for (let i = 2; i <= count; i++) {
  evaluate(i);
  // Reading the first entry keeps it alive while the second one ages out.
  findSourceMap('file:///generated-0.js');
}

assert.ok(findSourceMap('file:///generated-0.js'));
assert.strictEqual(findSourceMap('file:///generated-1.js'), undefined);

const sourceMap = findSourceMap(`file:///generated-${count}.js`);
assert.ok(sourceMap);
assert.strictEqual(sourceMap.findEntry(0, 0).originalLine, 0);
