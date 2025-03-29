'use strict';
// This tests heap snapshot integration of URLPattern.
require('../common');
const { findByRetainingPath } = require('../common/heap');
const { URLPattern } = require('node:url');
const assert = require('assert');

// Before url pattern is used, no URLPattern should be created.
{
  const nodes = findByRetainingPath('Node / URLPattern', []);
  assert.strictEqual(nodes.length, 0);
}

const urlPattern = new URLPattern('https://example.com/:id');

// After url pattern is used, a URLPattern should be created.
findByRetainingPath('Node / URLPattern', [
  { node_name: 'Node / ada::url_pattern', edge_name: 'url_pattern' },
  // ada::url_pattern references ada::url_pattern_component.
  { node_name: 'Node / ada::url_pattern_component', edge_name: 'protocol_component' },
]);

// Use `urlPattern`.
console.log(urlPattern);
