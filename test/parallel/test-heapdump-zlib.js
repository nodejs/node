// Flags: --expose-internals
'use strict';
require('../common');
const { validateSnapshotNodes } = require('../common/heap');
const zlib = require('zlib');

validateSnapshotNodes('ZCtx', []);
// eslint-disable-next-line no-unused-vars
const gunzip = zlib.createGunzip();
validateSnapshotNodes('ZCtx', [
  {
    children: [
      { name: 'Zlib' },
      { name: 'zlib memory' }
    ]
  }
]);
