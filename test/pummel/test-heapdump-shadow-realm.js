// Flags: --experimental-shadow-realm --expose-internals
'use strict';
require('../common');
const { validateSnapshotNodes } = require('../common/heap');

validateSnapshotNodes('Node / ShadowRealm', []);

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
  validateSnapshotNodes('Node / Environment', [
    {
      children: [
        { node_name: 'Node / shadow_realms', edge_name: 'shadow_realms' },
      ],
    },
  ]);
  validateSnapshotNodes('Node / shadow_realms', [
    {
      children: [
        { node_name: 'Node / ShadowRealm' },
      ],
    },
  ]);
}

createRealms();
