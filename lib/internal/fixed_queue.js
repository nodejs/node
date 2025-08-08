'use strict';

const {
  Array,
  ArrayPrototypeFill,
} = primordials;

// Currently optimal queue size, tested on V8 6.0 - 6.6. Must be power of two.
const kSize = 2048;
const kMask = kSize - 1;

// Buffer pool to reduce allocations - optimization for nextTick performance
const kMaxPoolSize = 16; // Keep reasonable number of buffers in pool
let bufferPool = [];
let bufferPoolSize = 0;

// The FixedQueue is implemented as a singly-linked list of fixed-size
// circular buffers. It looks something like this:
//
//  head                                                       tail
//    |                                                          |
//    v                                                          v
// +-----------+ <-----\       +-----------+ <------\         +-----------+
// |  [null]   |        \----- |   next    |         \------- |   next    |
// +-----------+               +-----------+                  +-----------+
// |   item    | <-- bottom    |   item    | <-- bottom       | undefined |
// |   item    |               |   item    |                  | undefined |
// |   item    |               |   item    |                  | undefined |
// |   item    |               |   item    |                  | undefined |
// |   item    |               |   item    |       bottom --> |   item    |
// |   item    |               |   item    |                  |   item    |
// |    ...    |               |    ...    |                  |    ...    |
// |   item    |               |   item    |                  |   item    |
// |   item    |               |   item    |                  |   item    |
// | undefined | <-- top       |   item    |                  |   item    |
// | undefined |               |   item    |                  |   item    |
// | undefined |               | undefined | <-- top  top --> | undefined |
// +-----------+               +-----------+                  +-----------+
//
// Or, if there is only one circular buffer, it looks something
// like either of these:
//
//  head   tail                                 head   tail
//    |     |                                     |     |
//    v     v                                     v     v
// +-----------+                               +-----------+
// |  [null]   |                               |  [null]   |
// +-----------+                               +-----------+
// | undefined |                               |   item    |
// | undefined |                               |   item    |
// |   item    | <-- bottom            top --> | undefined |
// |   item    |                               | undefined |
// | undefined | <-- top            bottom --> |   item    |
// | undefined |                               |   item    |
// +-----------+                               +-----------+
//
// Adding a value means moving `top` forward by one, removing means
// moving `bottom` forward by one. After reaching the end, the queue
// wraps around.
//
// When `top === bottom` the current queue is empty and when
// `top + 1 === bottom` it's full. This wastes a single space of storage
// but allows much quicker checks.

// Buffer pool functions for optimization
function getPooledBuffer() {
  if (bufferPoolSize > 0) {
    const buffer = bufferPool[--bufferPoolSize];
    // Reset buffer state for reuse
    buffer.bottom = 0;
    buffer.top = 0;
    buffer.next = null;
    ArrayPrototypeFill(buffer.list, undefined);
    return buffer;
  }
  return new FixedCircularBuffer();
}

function releaseBuffer(buffer) {
  if (bufferPoolSize < kMaxPoolSize) {
    bufferPool[bufferPoolSize++] = buffer;
  }
}

class FixedCircularBuffer {
  constructor() {
    this.bottom = 0;
    this.top = 0;
    this.list = ArrayPrototypeFill(new Array(kSize), undefined);
    this.next = null;
  }

  isEmpty() {
    return this.top === this.bottom;
  }

  isFull() {
    return ((this.top + 1) & kMask) === this.bottom;
  }

  push(data) {
    this.list[this.top] = data;
    this.top = (this.top + 1) & kMask;
  }

  shift() {
    const nextItem = this.list[this.bottom];
    if (nextItem === undefined)
      return null;
    this.list[this.bottom] = undefined;
    this.bottom = (this.bottom + 1) & kMask;
    return nextItem;
  }

  // Optimized batch shift for better cache locality
  shiftBatch(destination, maxCount) {
    let count = 0;
    while (count < maxCount && !this.isEmpty()) {
      const item = this.list[this.bottom];
      if (item === undefined) break;
      
      destination[count++] = item;
      this.list[this.bottom] = undefined;
      this.bottom = (this.bottom + 1) & kMask;
    }
    return count;
  }
}

module.exports = class FixedQueue {
  constructor() {
    this.head = this.tail = new FixedCircularBuffer();
    // Batch processing buffer for optimization
    this.batchBuffer = new Array(64);
  }

  isEmpty() {
    return this.head.isEmpty();
  }

  push(data) {
    if (this.head.isFull()) {
      // Head is full: Use pooled buffer instead of always creating new
      this.head = this.head.next = getPooledBuffer();
    }
    this.head.push(data);
  }

  shift() {
    const tail = this.tail;
    const next = tail.shift();
    if (tail.isEmpty() && tail.next !== null) {
      // Return old tail to pool for reuse
      releaseBuffer(this.tail);
      this.tail = tail.next;
      tail.next = null;
    }
    return next;
  }

  // Optimized batch processing method
  processBatch(processor, maxBatch = 64) {
    let processed = 0;
    
    while (processed < maxBatch && !this.isEmpty()) {
      const tail = this.tail;
      const batchCount = tail.shiftBatch(this.batchBuffer, maxBatch - processed);
      
      if (batchCount === 0) break;
      
      // Process batch with better cache locality
      for (let i = 0; i < batchCount; i++) {
        processor(this.batchBuffer[i]);
        this.batchBuffer[i] = undefined; // Clear reference
      }
      
      processed += batchCount;
      
      // Check if we need to move to next buffer
      if (tail.isEmpty() && tail.next !== null) {
        releaseBuffer(this.tail);
        this.tail = tail.next;
        tail.next = null;
      }
    }
    
    return processed;
  }
};
