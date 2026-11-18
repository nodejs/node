'use strict';

const assert = require('assert');
const { findSourceMap } = require('module');

const sm = findSourceMap(__filename);
assert.ok(sm, 'source map should be loaded from the SEA VFS');
console.log('source map loaded from the SEA VFS:', sm.payload.sources[0]);

function boom() {
    throw new Error('boom from the original source');
}

boom();
