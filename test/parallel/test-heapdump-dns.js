// Flags: --expose-internals
'use strict';
require('../common');
const { validateSnapshotNodes } = require('../common/heap');

validateSnapshotNodes('DNSCHANNEL', []);
const dns = require('dns');
validateSnapshotNodes('DNSCHANNEL', [{}]);
dns.resolve('localhost', () => {});
validateSnapshotNodes('DNSCHANNEL', [
  {
    children: [
      { name: 'task list' },
      { name: 'ChannelWrap' }
    ]
  }
]);
