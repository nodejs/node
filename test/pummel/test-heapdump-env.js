// Flags: --expose-internals
'use strict';

// This tests that Environment is tracked in heap snapshots.

require('../common');
const { validateSnapshotNodes } = require('../common/heap');

// This is just using ContextifyScript as an example here, it can be replaced
// with any BaseObject that we can easily instantiate here and register in
// cleanup hooks.
// These can all be changed to reflect the status of how these objects
// are captured in the snapshot.
const context = require('vm').createScript('const foo = 123');

validateSnapshotNodes('Node / Environment', [{
  children: [
    { node_name: 'Node / CleanupQueue', edge_name: 'cleanup_queue' },
    { node_name: 'Node / IsolateData', edge_name: 'isolate_data' },
    { node_name: 'Node / PrincipalRealm', edge_name: 'principal_realm' },
  ],
}]);

validateSnapshotNodes('Node / CleanupQueue', [
  // The cleanup_queue of the Environment.
  {},
  // No other CleanupQueues in the snapshot.
]);

validateSnapshotNodes('Node / PrincipalRealm', [{
  children: [
    { node_name: 'process', edge_name: 'process_object' },
    { node_name: 'Node / BaseObjectList', edge_name: 'base_object_list' },
  ],
}]);

console.log(context);  // Make sure it's not GC'ed
