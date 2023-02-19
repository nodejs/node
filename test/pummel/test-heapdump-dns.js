// Flags: --expose-internals
'use strict';
require('../common');
const { validateSnapshotNodes } = require('../common/heap');

validateSnapshotNodes('Node / ChannelWrap', []);
const dns = require('dns');
validateSnapshotNodes('Node / ChannelWrap', [{}]);
dns.resolve('localhost', () => {});
validateSnapshotNodes('Node / ChannelWrap', [
  {
    children: [
      { node_name: 'Node / NodeAresTask::List', edge_name: 'task_list' },
      // `Node / ChannelWrap` (C++) -> `ChannelWrap` (JS)
      { node_name: 'ChannelWrap', edge_name: 'native_to_javascript' },
    ],
    detachedness: 2,
  },
]);
