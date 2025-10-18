'use strict';
require('../common');
const { validateByRetainingPath } = require('../common/heap');
const { createHash } = require('crypto');
const assert = require('assert');

// In case the bootstrap process creates any Hash objects, capture a snapshot first
// and save the initial length.
const originalNodes = validateByRetainingPath('Node / Hash', [
  { edge_name: 'mdctx' },
], true);

const count = 5;
const arr = [];
for (let i = 0; i < count; ++i) {
  arr.push(createHash('sha1'));
}

const nodesWithCtx = validateByRetainingPath('Node / Hash', [
  { edge_name: 'mdctx', node_name: 'Node / EVP_MD_CTX' },
]);

assert.strictEqual(nodesWithCtx.length - originalNodes.length, count);

for (let i = 0; i < count; ++i) {
  arr[i].update('test').digest('hex');
}

const nodesWithDigest = validateByRetainingPath('Node / Hash', [
  { edge_name: 'md', node_name: 'Node / ByteSource' },
]);

assert.strictEqual(nodesWithDigest.length - originalNodes.length, count);
