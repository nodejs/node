// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const streamNew = require('stream/iter');

// =============================================================================
// Stream namespace object
// =============================================================================

async function testStreamNamespaceExists() {
  assert.ok(streamNew.Stream);
  assert.strictEqual(typeof streamNew.Stream, 'object');
}

async function testStreamNamespaceFrozen() {
  assert.ok(Object.isFrozen(streamNew.Stream));
}

async function testStreamNamespaceFactories() {
  const { Stream } = streamNew;

  assert.strictEqual(typeof Stream.push, 'function');
  assert.strictEqual(typeof Stream.duplex, 'function');
  assert.strictEqual(typeof Stream.from, 'function');
  assert.strictEqual(typeof Stream.fromSync, 'function');
}

async function testStreamNamespacePipelines() {
  const { Stream } = streamNew;

  assert.strictEqual(typeof Stream.pull, 'function');
  assert.strictEqual(typeof Stream.pullSync, 'function');
  assert.strictEqual(typeof Stream.pipeTo, 'function');
  assert.strictEqual(typeof Stream.pipeToSync, 'function');
}

async function testStreamNamespaceAsyncConsumers() {
  const { Stream } = streamNew;

  assert.strictEqual(typeof Stream.bytes, 'function');
  assert.strictEqual(typeof Stream.text, 'function');
  assert.strictEqual(typeof Stream.arrayBuffer, 'function');
  assert.strictEqual(typeof Stream.array, 'function');
}

async function testStreamNamespaceSyncConsumers() {
  const { Stream } = streamNew;

  assert.strictEqual(typeof Stream.bytesSync, 'function');
  assert.strictEqual(typeof Stream.textSync, 'function');
  assert.strictEqual(typeof Stream.arrayBufferSync, 'function');
  assert.strictEqual(typeof Stream.arraySync, 'function');
}

async function testStreamNamespaceCombining() {
  const { Stream } = streamNew;

  assert.strictEqual(typeof Stream.merge, 'function');
  assert.strictEqual(typeof Stream.broadcast, 'function');
  assert.strictEqual(typeof Stream.share, 'function');
  assert.strictEqual(typeof Stream.shareSync, 'function');
}

async function testStreamNamespaceUtilities() {
  const { Stream } = streamNew;

  assert.strictEqual(typeof Stream.tap, 'function');
  assert.strictEqual(typeof Stream.tapSync, 'function');
  assert.strictEqual(typeof Stream.ondrain, 'function');
}

async function testStreamNamespaceProtocols() {
  const { Stream } = streamNew;

  assert.strictEqual(typeof Stream.toStreamable, 'symbol');
  assert.strictEqual(typeof Stream.toAsyncStreamable, 'symbol');
  assert.strictEqual(typeof Stream.broadcastProtocol, 'symbol');
  assert.strictEqual(typeof Stream.shareProtocol, 'symbol');
  assert.strictEqual(typeof Stream.shareSyncProtocol, 'symbol');
  assert.strictEqual(typeof Stream.drainableProtocol, 'symbol');
}

// =============================================================================
// Individual exports (destructured imports)
// =============================================================================

async function testIndividualExports() {
  // Factories
  assert.strictEqual(typeof streamNew.push, 'function');
  assert.strictEqual(typeof streamNew.duplex, 'function');
  assert.strictEqual(typeof streamNew.from, 'function');
  assert.strictEqual(typeof streamNew.fromSync, 'function');

  // Pipelines
  assert.strictEqual(typeof streamNew.pull, 'function');
  assert.strictEqual(typeof streamNew.pullSync, 'function');
  assert.strictEqual(typeof streamNew.pipeTo, 'function');
  assert.strictEqual(typeof streamNew.pipeToSync, 'function');

  // Consumers
  assert.strictEqual(typeof streamNew.bytes, 'function');
  assert.strictEqual(typeof streamNew.bytesSync, 'function');
  assert.strictEqual(typeof streamNew.text, 'function');
  assert.strictEqual(typeof streamNew.textSync, 'function');
  assert.strictEqual(typeof streamNew.arrayBuffer, 'function');
  assert.strictEqual(typeof streamNew.arrayBufferSync, 'function');
  assert.strictEqual(typeof streamNew.array, 'function');
  assert.strictEqual(typeof streamNew.arraySync, 'function');

  // Combining
  assert.strictEqual(typeof streamNew.merge, 'function');
  assert.strictEqual(typeof streamNew.broadcast, 'function');
  assert.strictEqual(typeof streamNew.share, 'function');
  assert.strictEqual(typeof streamNew.shareSync, 'function');

  // Utilities
  assert.strictEqual(typeof streamNew.tap, 'function');
  assert.strictEqual(typeof streamNew.tapSync, 'function');
  assert.strictEqual(typeof streamNew.ondrain, 'function');

  // Protocol symbols
  assert.strictEqual(typeof streamNew.toStreamable, 'symbol');
  assert.strictEqual(typeof streamNew.toAsyncStreamable, 'symbol');
  assert.strictEqual(typeof streamNew.broadcastProtocol, 'symbol');
  assert.strictEqual(typeof streamNew.shareProtocol, 'symbol');
  assert.strictEqual(typeof streamNew.shareSyncProtocol, 'symbol');
  assert.strictEqual(typeof streamNew.drainableProtocol, 'symbol');
}

async function testMultiConsumerExports() {
  // Broadcast and Share constructors/factories
  assert.ok(streamNew.Broadcast);
  assert.strictEqual(typeof streamNew.Broadcast.from, 'function');
  assert.ok(streamNew.Share);
  assert.strictEqual(typeof streamNew.Share.from, 'function');
  assert.ok(streamNew.SyncShare);
  assert.strictEqual(typeof streamNew.SyncShare.fromSync, 'function');
}

// =============================================================================
// Cross-check: namespace matches individual exports
// =============================================================================

async function testNamespaceMatchesExports() {
  const { Stream } = streamNew;

  // Every function on Stream should also be available as a direct export
  assert.strictEqual(Stream.push, streamNew.push);
  assert.strictEqual(Stream.duplex, streamNew.duplex);
  assert.strictEqual(Stream.from, streamNew.from);
  assert.strictEqual(Stream.fromSync, streamNew.fromSync);
  assert.strictEqual(Stream.pull, streamNew.pull);
  assert.strictEqual(Stream.pullSync, streamNew.pullSync);
  assert.strictEqual(Stream.pipeTo, streamNew.pipeTo);
  assert.strictEqual(Stream.pipeToSync, streamNew.pipeToSync);
  assert.strictEqual(Stream.bytes, streamNew.bytes);
  assert.strictEqual(Stream.text, streamNew.text);
  assert.strictEqual(Stream.arrayBuffer, streamNew.arrayBuffer);
  assert.strictEqual(Stream.array, streamNew.array);
  assert.strictEqual(Stream.bytesSync, streamNew.bytesSync);
  assert.strictEqual(Stream.textSync, streamNew.textSync);
  assert.strictEqual(Stream.arrayBufferSync, streamNew.arrayBufferSync);
  assert.strictEqual(Stream.arraySync, streamNew.arraySync);
  assert.strictEqual(Stream.merge, streamNew.merge);
  assert.strictEqual(Stream.broadcast, streamNew.broadcast);
  assert.strictEqual(Stream.share, streamNew.share);
  assert.strictEqual(Stream.shareSync, streamNew.shareSync);
  assert.strictEqual(Stream.tap, streamNew.tap);
  assert.strictEqual(Stream.tapSync, streamNew.tapSync);
  assert.strictEqual(Stream.ondrain, streamNew.ondrain);

  // Protocol symbols
  assert.strictEqual(Stream.toStreamable, streamNew.toStreamable);
  assert.strictEqual(Stream.toAsyncStreamable, streamNew.toAsyncStreamable);
  assert.strictEqual(Stream.broadcastProtocol, streamNew.broadcastProtocol);
  assert.strictEqual(Stream.shareProtocol, streamNew.shareProtocol);
  assert.strictEqual(Stream.shareSyncProtocol, streamNew.shareSyncProtocol);
  assert.strictEqual(Stream.drainableProtocol, streamNew.drainableProtocol);
}

// =============================================================================
// Require paths
// =============================================================================

async function testRequirePaths() {
  // Both require('stream/iter') and require('node:stream/iter') should work
  const fromPlain = require('stream/iter');
  const fromNode = require('node:stream/iter');

  assert.strictEqual(fromPlain.Stream, fromNode.Stream);
  assert.strictEqual(fromPlain.push, fromNode.push);
}

Promise.all([
  testStreamNamespaceExists(),
  testStreamNamespaceFrozen(),
  testStreamNamespaceFactories(),
  testStreamNamespacePipelines(),
  testStreamNamespaceAsyncConsumers(),
  testStreamNamespaceSyncConsumers(),
  testStreamNamespaceCombining(),
  testStreamNamespaceUtilities(),
  testStreamNamespaceProtocols(),
  testIndividualExports(),
  testMultiConsumerExports(),
  testNamespaceMatchesExports(),
  testRequirePaths(),
]).then(common.mustCall());
