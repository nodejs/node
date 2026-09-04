'use strict';
require('../common');
const assert = require('assert');
const { SourceMap } = require('node:module');

// Refs: https://github.com/nodejs/node/issues/63795

// A mapping whose final segment has 4 fields (no name index) must not
// inherit the previous segment's name.
{
  const sm = new SourceMap({
    version: 3,
    sources: ['a.js'],
    names: ['foo'],
    mappings: 'AAAAA,CAAC',
  });

  assert.strictEqual(sm.findEntry(0, 0).name, 'foo');
  assert.strictEqual(sm.findEntry(0, 1).name, undefined);
}

// A mapping whose final segment legitimately carries a name index must still
// resolve it: the end-of-string guard must not suppress a real trailing name.
{
  const sm = new SourceMap({
    version: 3,
    sources: ['a.js'],
    names: ['foo'],
    mappings: 'AAAA,CAAAA',
  });

  assert.strictEqual(sm.findEntry(0, 0).name, undefined);
  assert.strictEqual(sm.findEntry(0, 1).name, 'foo');
}
