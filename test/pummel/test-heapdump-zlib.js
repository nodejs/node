'use strict';
// This tests heap snapshot integration of zlib stream.

const common = require('../common');
const assert = require('assert');
const { validateByRetainingPath, validateByRetainingPathFromNodes } = require('../common/heap');
const zlib = require('zlib');

// Before zlib stream is created, no ZlibStream should be created.
{
  const nodes = validateByRetainingPath('Node / ZlibStream', []);
  assert.strictEqual(nodes.length, 0);
}

const gzip = zlib.createGzip();

// After zlib stream is created, a ZlibStream should be created.
{
  const streams = validateByRetainingPath('Node / ZlibStream', []);
  validateByRetainingPathFromNodes(streams, 'Node / ZlibStream', [
    { node_name: 'Zlib', edge_name: 'native_to_javascript' },
  ]);
  // No entry for memory because zlib memory is initialized lazily.
  const withMemory = validateByRetainingPathFromNodes(streams, 'Node / ZlibStream', [
    { node_name: 'Node / zlib_memory', edge_name: 'zlib_memory' },
  ], true);
  assert.strictEqual(withMemory.length, 0);
}

// After zlib stream is written, zlib_memory should be created.
gzip.write('hello world', common.mustCall(() => {
  validateByRetainingPath('Node / ZlibStream', [
    { node_name: 'Node / zlib_memory', edge_name: 'zlib_memory' },
  ]);
}));
