// Flags: --expose-internals
'use strict';
require('../common');
const { validateSnapshotNodes } = require('../common/heap');
const fs = require('fs').promises;

validateSnapshotNodes('FSREQPROMISE', []);
fs.stat(__filename);
validateSnapshotNodes('FSREQPROMISE', [
  {
    children: [
      { name: 'FSReqPromise' },
      { name: 'Float64Array' }  // Stat array
    ]
  }
]);
