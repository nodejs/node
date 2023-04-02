// Flags: --experimental-shadow-realm --expose-internals
'use strict';
require('../common');
const { validateSnapshotNodes } = require('../common/heap');

validateSnapshotNodes('Node / ShadowRealm', []);
const realm = new ShadowRealm();
{
  // Create a bunch of un-referenced ShadowRealms to make sure the heap
  // snapshot can handle it.
  for (let i = 0; i < 100; i++) {
    const realm = new ShadowRealm();
    realm.evaluate('undefined');
  }
}
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

// Keep the realm alive.
realm.evaluate('undefined');
