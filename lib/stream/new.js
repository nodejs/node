'use strict';

// Public entry point for the new streams API.
// Usage: require('stream/new') or require('node:stream/new')

const {
  ObjectFreeze,
} = primordials;

// Protocol symbols
const {
  toStreamable,
  toAsyncStreamable,
  broadcastProtocol,
  shareProtocol,
  shareSyncProtocol,
  drainableProtocol,
} = require('internal/streams/new/types');

// Factories
const { push } = require('internal/streams/new/push');
const { duplex } = require('internal/streams/new/duplex');
const { from, fromSync } = require('internal/streams/new/from');

// Pipelines
const {
  pull,
  pullSync,
  pipeTo,
  pipeToSync,
} = require('internal/streams/new/pull');

// Consumers
const {
  bytes,
  bytesSync,
  text,
  textSync,
  arrayBuffer,
  arrayBufferSync,
  array,
  arraySync,
  tap,
  tapSync,
  merge,
  ondrain,
} = require('internal/streams/new/consumers');

// Transforms
const {
  compressGzip,
  compressDeflate,
  compressBrotli,
  compressZstd,
  decompressGzip,
  decompressDeflate,
  decompressBrotli,
  decompressZstd,
} = require('internal/streams/new/transform');

// Multi-consumer
const { broadcast, Broadcast } = require('internal/streams/new/broadcast');
const {
  share,
  shareSync,
  Share,
  SyncShare,
} = require('internal/streams/new/share');

/**
 * Stream namespace - unified access to all stream functions.
 * @example
 * const { Stream } = require('stream/new');
 *
 * const { writer, readable } = Stream.push();
 * await writer.write("hello");
 * await writer.end();
 *
 * const output = Stream.pull(readable, transform1, transform2);
 * const data = await Stream.bytes(output);
 */
const Stream = ObjectFreeze({
  // Factories
  push,
  duplex,
  from,
  fromSync,

  // Pipelines
  pull,
  pullSync,

  // Pipe to destination
  pipeTo,
  pipeToSync,

  // Consumers (async)
  bytes,
  text,
  arrayBuffer,
  array,

  // Consumers (sync)
  bytesSync,
  textSync,
  arrayBufferSync,
  arraySync,

  // Combining
  merge,

  // Multi-consumer (push model)
  broadcast,

  // Multi-consumer (pull model)
  share,
  shareSync,

  // Utilities
  tap,
  tapSync,

  // Drain utility for event source integration
  ondrain,

  // Compression / decompression transforms
  compressGzip,
  compressDeflate,
  compressBrotli,
  compressZstd,
  decompressGzip,
  decompressDeflate,
  decompressBrotli,
  decompressZstd,

  // Protocol symbols
  toStreamable,
  toAsyncStreamable,
  broadcastProtocol,
  shareProtocol,
  shareSyncProtocol,
  drainableProtocol,
});

module.exports = {
  // The Stream namespace
  Stream,

  // Also export everything individually for destructured imports

  // Protocol symbols
  toStreamable,
  toAsyncStreamable,
  broadcastProtocol,
  shareProtocol,
  shareSyncProtocol,
  drainableProtocol,

  // Factories
  push,
  duplex,
  from,
  fromSync,

  // Pipelines
  pull,
  pullSync,
  pipeTo,
  pipeToSync,

  // Consumers (async)
  bytes,
  text,
  arrayBuffer,
  array,

  // Consumers (sync)
  bytesSync,
  textSync,
  arrayBufferSync,
  arraySync,

  // Combining
  merge,

  // Multi-consumer
  broadcast,
  Broadcast,
  share,
  shareSync,
  Share,
  SyncShare,

  // Utilities
  tap,
  tapSync,
  ondrain,

  // Compression / decompression transforms
  compressGzip,
  compressDeflate,
  compressBrotli,
  compressZstd,
  decompressGzip,
  decompressDeflate,
  decompressBrotli,
  decompressZstd,
};
