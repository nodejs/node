'use strict';

// New Streams API - Protocol Symbols
//
// These symbols allow objects to participate in streaming.
// Using Symbol.for() allows third-party code to implement protocols
// without importing these symbols directly.

const {
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

module.exports = {
  toStreamable,
  toAsyncStreamable,
  broadcastProtocol,
  shareProtocol,
  shareSyncProtocol,
  drainableProtocol,
};
