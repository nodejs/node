// Flags: --expose-internals
'use strict';
require('../common');
const { validateSnapshotNodes } = require('../common/heap');
const fs = require('fs').promises;

validateSnapshotNodes('FSReqPromise', []);
fs.stat(__filename);
validateSnapshotNodes('FSReqPromise', [
  {
    children: [
      { name: 'FSReqPromise' },
      { name: 'Float64Array' }  // Stat array
    ]
  }
]);
