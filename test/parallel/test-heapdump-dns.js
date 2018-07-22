// Flags: --expose-internals
'use strict';
require('../common');
const { validateSnapshotNodes } = require('../common/heap');

validateSnapshotNodes('ChannelWrap', []);
const dns = require('dns');
validateSnapshotNodes('ChannelWrap', [{}]);
dns.resolve('localhost', () => {});
validateSnapshotNodes('ChannelWrap', [
  {
    children: [
      { name: 'node_ares_task_list' },
      // `Node / ChannelWrap` (C++) -> `ChannelWrap` (JS)
      { name: 'ChannelWrap' }
    ]
  }
]);
