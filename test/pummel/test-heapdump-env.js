'use strict';

// This tests that Environment is tracked in heap snapshots.
// Tests for BaseObject and cppgc-managed objects are done in other
// test-heapdump-*.js files.

require('../common');
const { createJSHeapSnapshot, validateByRetainingPathFromNodes } = require('../common/heap');

const nodes = createJSHeapSnapshot();

const envs = validateByRetainingPathFromNodes(nodes, 'Node / Environment', []);
validateByRetainingPathFromNodes(envs, 'Node / Environment', [
  { node_name: 'Node / CleanupQueue', edge_name: 'cleanup_queue' },
]);
validateByRetainingPathFromNodes(envs, 'Node / Environment', [
  { node_name: 'Node / IsolateData', edge_name: 'isolate_data' },
]);

const realms = validateByRetainingPathFromNodes(envs, 'Node / Environment', [
  { node_name: 'Node / PrincipalRealm', edge_name: 'principal_realm' },
]);
validateByRetainingPathFromNodes(realms, 'Node / PrincipalRealm', [
  { node_name: 'process', edge_name: 'process_object' },
]);
validateByRetainingPathFromNodes(realms, 'Node / PrincipalRealm', [
  { node_name: 'Node / BaseObjectList', edge_name: 'base_object_list' },
]);
