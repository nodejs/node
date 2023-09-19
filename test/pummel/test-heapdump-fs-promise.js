// Flags: --expose-internals
'use strict';
require('../common');
const { validateSnapshotNodes } = require('../common/heap');
const fs = require('fs').promises;

validateSnapshotNodes('Node / FSReqPromise', []);
fs.stat(__filename);
validateSnapshotNodes('Node / FSReqPromise', [
  {
    children: [
      { node_name: 'FSReqPromise', edge_name: 'native_to_javascript' },
      { node_name: 'Node / AliasedFloat64Array', edge_name: 'stats_field_array' },
    ],
  },
]);
