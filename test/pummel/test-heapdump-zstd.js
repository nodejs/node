'use strict';
// This tests heap snapshot integration of zlib stream.

const common = require('../common');
const assert = require('assert');
const { validateByRetainingPath, validateByRetainingPathFromNodes } = require('../common/heap');
const zlib = require('zlib');

// Before zstd stream is created, no ZstdStream should be created.
{
  const nodes = validateByRetainingPath('Node / ZstdStream', []);
  assert.strictEqual(nodes.length, 0);
}

const compress = zlib.createZstdCompress();

// After zstd stream is created, a ZstdStream should be created.
{
  const streams = validateByRetainingPath('Node / ZstdStream', []);
  validateByRetainingPathFromNodes(streams, 'Node / ZstdStream', [
    { node_name: 'ZstdCompress', edge_name: 'native_to_javascript' },
  ]);
  const withMemory = validateByRetainingPathFromNodes(streams, 'Node / ZstdStream', [
    { node_name: 'Node / zlib_memory', edge_name: 'zlib_memory' },
  ], true);
  if (process.config.variables.node_shared_zstd === true) {
    assert.strictEqual(withMemory.length, 0);
  } else {
    assert.strictEqual(withMemory.length, 1);
    // Between 1KB and 1MB (measured value was around ~5kB)
    assert.ok(withMemory[0].self_size > 1024);
    assert.ok(withMemory[0].self_size < 1024 * 1024);
  }
}

// After zstd stream is written, zlib_memory is significantly larger.
compress.write('hello world', common.mustCall(() => {
  const streams = validateByRetainingPath('Node / ZstdStream', []);
  const withMemory = validateByRetainingPathFromNodes(streams, 'Node / ZstdStream', [
    { node_name: 'Node / zlib_memory', edge_name: 'zlib_memory' },
  ], true);
  if (process.config.variables.node_shared_zstd === true) {
    assert.strictEqual(withMemory.length, 0);
  } else {
    assert.strictEqual(withMemory.length, 1);
    // More than 1MB
    assert.ok(withMemory[0].self_size > 1024 * 1024);
  }
}));
