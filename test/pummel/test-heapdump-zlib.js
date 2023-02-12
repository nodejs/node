// Flags: --expose-internals
'use strict';
const common = require('../common');
const { validateSnapshotNodes } = require('../common/heap');
const zlib = require('zlib');

validateSnapshotNodes('Node / ZlibStream', []);

const gzip = zlib.createGzip();
validateSnapshotNodes('Node / ZlibStream', [
  {
    children: [
      { node_name: 'Zlib', edge_name: 'native_to_javascript' },
      // No entry for memory because zlib memory is initialized lazily.
    ],
  },
]);

gzip.write('hello world', common.mustCall(() => {
  validateSnapshotNodes('Node / ZlibStream', [
    {
      children: [
        { node_name: 'Zlib', edge_name: 'native_to_javascript' },
        { node_name: 'Node / zlib_memory', edge_name: 'zlib_memory' },
      ],
    },
  ]);
}));
