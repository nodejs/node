'use strict';

const {
  Symbol,
  SymbolFor,
} = primordials;

/**
 * Symbol for sync value-to-streamable conversion protocol.
 * Objects implementing this can be written to streams or yielded
 * from generators. Works in both sync and async contexts.
 *
 * Third-party: [Symbol.for('Stream.toStreamable')]() { ... }
 */
const toStreamable = SymbolFor('Stream.toStreamable');

/**
 * Symbol for async value-to-streamable conversion protocol.
 * Objects implementing this can be written to async streams.
 * Works in async contexts only.
 *
 * Third-party: [Symbol.for('Stream.toAsyncStreamable')]() { ... }
 */
const toAsyncStreamable = SymbolFor('Stream.toAsyncStreamable');

/**
 * Symbol for Broadcastable protocol - object can provide a Broadcast.
 */
const broadcastProtocol = SymbolFor('Stream.broadcastProtocol');

/**
 * Symbol for Shareable protocol - object can provide a Share.
 */
const shareProtocol = SymbolFor('Stream.shareProtocol');

/**
 * Symbol for SyncShareable protocol - object can provide a SyncShare.
 */
const shareSyncProtocol = SymbolFor('Stream.shareSyncProtocol');

/**
 * Symbol for Drainable protocol - object can signal when backpressure
 * clears. Used to bridge event-driven sources that need drain notification.
 */
const drainableProtocol = SymbolFor('Stream.drainableProtocol');

/**
 * Internal sentinel for validated stateful transforms. A transform object
 * with [kValidatedTransform] = true signals that:
 *   1. It handles source exhaustion (done) internally - no withFlushAsync
 *      wrapper needed.
 *   2. It always yields valid Uint8Array[] batches - no isUint8ArrayBatch
 *      validation needed on each yield.
 * This is NOT a public protocol symbol - it uses Symbol() not Symbol.for().
 */
const kValidatedTransform = Symbol('kValidatedTransform');

/**
 * Internal sentinel for validated sources. An async iterable with
 * [kValidatedSource] = true signals that it already yields valid
 * Uint8Array[] batches - no normalizeAsyncSource wrapper needed.
 * from() will return such sources directly, skipping all normalization.
 * This is NOT a public protocol symbol - it uses Symbol() not Symbol.for().
 */
const kValidatedSource = Symbol('kValidatedSource');

module.exports = {
  broadcastProtocol,
  drainableProtocol,
  kValidatedSource,
  kValidatedTransform,
  shareProtocol,
  shareSyncProtocol,
  toAsyncStreamable,
  toStreamable,
};
