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
  // Validate that ShadowRealm appears in the heap snapshot.
  // We don't validate a specific retaining path since ShadowRealms are now
  // managed via weak references and cleanup hooks rather than a strong
  // shadow_realms_ set (which was removed to fix the memory leak).
  const nodes = validateByRetainingPath('Node / ShadowRealm', []);
  assert(nodes.length > 0, 'Expected at least one ShadowRealm in heap snapshot');
}

createRealms();
