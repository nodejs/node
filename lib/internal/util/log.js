'use strict';

const {
  ArrayPrototypeJoin,
  ArrayPrototypePush,
} = primordials;

const { format } = require('internal/util/inspect');
const { guessHandleType } = internalBinding('util');
const {
  clearTimeout,
  setTimeout,
} = require('timers');

// Lazy-load binding to avoid circular dependencies
let binding;
function getBinding() {
  binding ||= internalBinding('util');
  return binding;
}

// Buffer configuration
const kBufferSize = 16384; // 16KB buffer
const kFlushInterval = 10; // Flush every 10ms
let pendingBuffer = [];
let pendingBytes = 0;
let flushTimer = null;
let isStdoutTTY = null;

// Check if stdout is TTY (only check once)
function isTTY() {
  if (isStdoutTTY === null) {
    // fd 1 is stdout
    const type = guessHandleType(1);
    // Type 1 is TTY, see node_util.cc GetUVHandleTypeCode
    isStdoutTTY = type === 1;
  }
  return isStdoutTTY;
}

// Flush pending buffer to C++
function flush() {
  if (flushTimer !== null) {
    clearTimeout(flushTimer);
    flushTimer = null;
  }

  if (pendingBuffer.length === 0) return;

  const toWrite = pendingBuffer;
  pendingBuffer = [];
  pendingBytes = 0;

  // Batch write: combine all strings and write once
  // This is much faster than individual write calls
  // Use join for better performance than string concatenation
  const combined = ArrayPrototypeJoin(toWrite, '\n') + '\n';

  getBinding().logRaw(combined);
}

// Schedule flush on next tick
function scheduleFlush() {
  if (flushTimer !== null) return;

  flushTimer = setTimeout(flush, kFlushInterval);
}

/**
 * High-performance logging function optimized for async buffering.
 * Unlike console.log, this function is designed for minimal overhead
 * when output is redirected to files or pipes.
 *
 * Behavior:
 * - TTY (terminal): Synchronous writes for immediate output
 * - Pipe/File: Async buffering for better performance
 * @param {...any} args - Values to log
 */
function log(...args) {
  if (args.length === 0) {
    getBinding().log('');
    return;
  }

  // Format arguments using util.format for consistency
  const formatted = format(...args);

  // If stdout is a TTY, write immediately (sync)
  // Otherwise, buffer for async writes (better performance)
  if (isTTY()) {
    getBinding().log(formatted);
  } else {
    // Async buffering path
    ArrayPrototypePush(pendingBuffer, formatted);
    pendingBytes += formatted.length;

    // Flush if buffer is full
    if (pendingBytes >= kBufferSize) {
      flush();
    } else {
      scheduleFlush();
    }
  }
}

// Ensure buffer is flushed before exit
process.on('beforeExit', flush);

module.exports = {
  log,
};
