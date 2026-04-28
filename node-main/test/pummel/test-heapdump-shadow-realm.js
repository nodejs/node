// Flags: --experimental-shadow-realm
// This tests heap snapshot integration of ShadowRealm

'use strict';
require('../common');
const { validateByRetainingPath } = require('../common/heap');
const assert = require('assert');

// Before shadow realm is created, no ShadowRealm should be captured.
{
  const nodes = validateByRetainingPath('Node / ShadowRealm', []);
  assert.strictEqual(nodes.length, 0);
}

let realm;
let counter = 0;
// Create a bunch of un-referenced ShadowRealms to make sure the heap
// snapshot can handle it.
function createRealms() {
  // Use setImmediate to give GC some time to kick in to avoid OOM.
  if (counter++ < 10) {
    realm = new ShadowRealm();
    realm.evaluate('undefined');
    setImmediate(createRealms);
  } else {
    validateHeap();
    // Keep the realm alive.
    realm.evaluate('undefined');
  }
}

function validateHeap() {
  validateByRetainingPath('Node / Environment', [
    { node_name: 'Node / shadow_realms', edge_name: 'shadow_realms' },
    { node_name: 'Node / ShadowRealm' },
  ]);
}

createRealms();
