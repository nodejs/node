'use strict';

const {
  Array,
  ArrayPrototypeFill,
} = primordials;

// Currently optimal queue size, tested on V8 6.0 - 6.6. Must be power of two.
const kSize = 2048;
const kMask = kSize - 1;

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
// Implementation detail: To reduce allocations, a single spare
// FixedCircularBuffer may be kept for reuse. When a segment empties, it
// is stored as the spare, and when the head fills, the spare is linked
// in. This does not change the active list structure or queue semantics.
//
// Adding a value means moving `top` forward by one, removing means
// moving `bottom` forward by one. After reaching the end, the queue
// wraps around.
//
// When `top === bottom` the current queue is empty and when
// `top + 1 === bottom` it's full. This wastes a single space of storage
// but allows much quicker checks.

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
    const top = this.top;
    this.list[top] = data;
    this.top = (top + 1) & kMask;
  }

  shift() {
    const bottom = this.bottom;
    const nextItem = this.list[bottom];
    if (nextItem === undefined)
      return null;
    this.list[bottom] = undefined;
    this.bottom = (bottom + 1) & kMask;
    return nextItem;
  }
}

module.exports = class FixedQueue {
  constructor() {
    this.head = this.tail = new FixedCircularBuffer();
    this._spare = null;
  }

  isEmpty() {
    return this.head.isEmpty();
  }

  push(data) {
    let head = this.head;
    if (head.isFull()) {
      // Head is full: reuse the spare buffer if available.
      // Otherwise create a new one. Link it and advance head.
      let nextBuf = this._spare;
      if (nextBuf !== null) {
        this._spare = null;
      } else {
        nextBuf = new FixedCircularBuffer();
      }
      this.head = head = head.next = nextBuf;
    }
    head.push(data);
  }

  shift() {
    const tail = this.tail;
    const next = tail.shift();
    if (tail.isEmpty() && tail.next !== null) {
      // If there is another queue, it forms the new tail.
      this.tail = tail.next;
      // Overwrite any existing spare
      this._spare = tail;
    }
    return next;
  }
};
