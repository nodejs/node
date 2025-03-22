'use strict';

// This tests heap snapshot integration of fs promise.

require('../common');
const { findByRetainingPath, findByRetainingPathFromNodes } = require('../common/heap');
const fs = require('fs').promises;
const assert = require('assert');

// Before fs promise is used, no FSReqPromise should be created.
{
  const nodes = findByRetainingPath('Node / FSReqPromise', []);
  assert.strictEqual(nodes.length, 0);
}

fs.stat(__filename);

{
  const nodes = findByRetainingPath('Node / FSReqPromise', []);
  findByRetainingPathFromNodes(nodes, 'Node / FSReqPromise', [
    { node_name: 'FSReqPromise', edge_name: 'native_to_javascript' },
  ]);
  findByRetainingPathFromNodes(nodes, 'Node / FSReqPromise', [
    { node_name: 'Node / AliasedFloat64Array', edge_name: 'stats_field_array' },
  ]);
}
