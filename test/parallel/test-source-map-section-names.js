'use strict';
require('../common');
const assert = require('assert');
const { SourceMap } = require('node:module');

// Names are looked up across sections, including sections that have no names.
const sm = new SourceMap({
  version: 3,
  sections: [
    {
      offset: { line: 0, column: 0 },
      map: { version: 3, sources: ['a.js'], names: ['a0', 'a1'],
             mappings: 'AAAAA,CAAAC' },
    },
    {
      offset: { line: 1, column: 0 },
      map: { version: 3, sources: ['b.js'], mappings: 'AAAA' },
    },
    {
      offset: { line: 2, column: 0 },
      map: { version: 3, sources: ['c.js'], names: [], mappings: 'AAAA' },
    },
    {
      offset: { line: 3, column: 0 },
      map: { version: 3, sources: ['d.js'], names: ['d0'],
             mappings: 'AAAAA' },
    },
  ],
});

assert.strictEqual(sm.findEntry(0, 0).name, 'a0');
assert.strictEqual(sm.findEntry(0, 1).name, 'a1');
assert.strictEqual(sm.findEntry(1, 0).name, undefined);
assert.strictEqual(sm.findEntry(2, 0).name, undefined);
assert.strictEqual(sm.findEntry(3, 0).name, 'd0');
