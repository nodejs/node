'use strict';

// This tests heap snapshot integration of fs promise.

require('../common');
const { validateByRetainingPath, validateByRetainingPathFromNodes } = require('../common/heap');
const fs = require('fs').promises;
const assert = require('assert');

// Before fs promise is used, no FSReqPromise should be created.
{
  const nodes = validateByRetainingPath('Node / FSReqPromise', []);
  assert.strictEqual(nodes.length, 0);
}

fs.stat(__filename);

{
  const nodes = validateByRetainingPath('Node / FSReqPromise', []);
  validateByRetainingPathFromNodes(nodes, 'Node / FSReqPromise', [
    { node_name: 'FSReqPromise', edge_name: 'native_to_javascript' },
  ]);
  validateByRetainingPathFromNodes(nodes, 'Node / FSReqPromise', [
    { node_name: 'Node / AliasedFloat64Array', edge_name: 'stats_field_array' },
  ]);
}
