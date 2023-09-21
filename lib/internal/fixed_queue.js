'use strict';

const {
  Array,
  SymbolIterator,
  TypedArrayPrototypeSet,
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
// |   item    | <-- bottom    |   item    | <-- bottom       |  [empty]  |
// |   item    |               |   item    |                  |  [empty]  |
// |   item    |               |   item    |                  |  [empty]  |
// |   item    |               |   item    |                  |  [empty]  |
// |   item    |               |   item    |       bottom --> |   item    |
// |   item    |               |   item    |                  |   item    |
// |    ...    |               |    ...    |                  |    ...    |
// |   item    |               |   item    |                  |   item    |
// |   item    |               |   item    |                  |   item    |
// |  [empty]  | <-- top       |   item    |                  |   item    |
// |  [empty]  |               |   item    |                  |   item    |
// |  [empty]  |               |  [empty]  | <-- top  top --> |  [empty]  |
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
// |  [empty]  |                               |   item    |
// |  [empty]  |                               |   item    |
// |   item    | <-- bottom            top --> |  [empty]  |
// |   item    |                               |  [empty]  |
// |  [empty]  | <-- top            bottom --> |   item    |
// |  [empty]  |                               |   item    |
// +-----------+                               +-----------+
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
    this.list = new Array(kSize);
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
  first() {
    return this.list[this.bottom] ?? null;
  }
  unshift(data) {
    this.bottom = (this.bottom - 1) & kMask;
    this.list[this.bottom] = data;
  }
  shift() {
    const nextItem = this.list[this.bottom];
    if (nextItem === undefined)
      return null;
    this.list[this.bottom] = undefined;
    this.bottom = (this.bottom + 1) & kMask;
    return nextItem;
  }
}

module.exports = class FixedQueue {
  constructor() {
    this.head = this.tail = new FixedCircularBuffer();
    this.length = 0;
  }

  isEmpty() {
    return this.head.isEmpty();
  }
  push(data) {
    if (this.head.isFull()) {
      // Head is full: Creates a new queue, sets the old queue's `.next` to it,
      // and sets it as the new main queue.
      this.head = this.head.next = new FixedCircularBuffer();
    }
    this.head.push(data);
    this.length += data.length;
  }
  clear() {
    this.head = this.tail = new FixedCircularBuffer();
    this.length = 0;
  }
  unshift(data) {
    if (this.tail.isFull()) {
      // Tail is full: Creates a new queue, and put it at the start
      // of the list.
      const newTail = new FixedCircularBuffer();
      newTail.next = this.tail;
      this.tail = newTail;
    }
    this.tail.unshift(data);
    this.length += data.length;
  }
  join() {
    let ret = '';
    for (let p = this.tail; p !== null; p = p.next) {
      for (let i = p.bottom; i !== p.top; i = (i + 1) & kMask) {
        ret += p.list[i];
      }
    }
    return ret;
  }
  concat(n) {
    const ret = Buffer.allocUnsafe(n >>> 0);
    let p = this.tail;
    let i = 0;
    while (p) {
      let slice;
      if (p.bottom < p.top) {
        slice = p.list.slice(p.bottom, p.top);
      } else {
        slice = p.list.slice(p.bottom).concat(p.list.slice(0, p.top));
      }
      TypedArrayPrototypeSet(ret, slice, i);
      i += slice.length;
      p = p.next;
    }
    return ret;
  }
  consume(n, hasStrings) {
    let ret;
    // n < this.tail.top - this.tail.bottom but accountng to othe fact
    // bottom might be more than top with kMask
    if (n < ((this.tail.top - this.tail.bottom) & kMask)) {
      // This slice is in the first buffer. Read it accounting for the fact
      // that bottom might be more than top with kMask.
      if (this.tail.bottom < this.tail.top) {
        ret = this.tail.list.slice(this.tail.bottom, this.tail.bottom + n);
      } else {
        ret = this.tail.list.slice(this.tail.bottom).concat(this.tail.list.slice(0, n));
      }
      this.tail.bottom = (this.tail.bottom + n) & kMask;
    } else if (n === ((this.tail.top - this.tail.bottom) & kMask)) {
      // This slice spans the first buffer exactly.
      if (this.tail.bottom < this.tail.top) {
        ret = this.tail.list.slice(this.tail.bottom, this.tail.top);
      } else {
        ret = this.tail.list.slice(this.tail.bottom)
                            .concat(this.tail.list.slice(0, n));
      }
      this.tail = this.tail.next;
    } else {
      // This slice spans multiple buffers.
      ret = Buffer.allocUnsafe(n);
      let i = 0;
      let p = this.tail;
      while (n > 0) {
        let slice;
        // account for the fact n might be more than top - bottom with kMask
        if (n < ((p.top - p.bottom) & kMask)) {
          slice = p.list.slice(p.bottom, p.bottom + n);
          p.bottom = (p.bottom + n) & kMask;
          n = 0;
        }
        if (p.bottom < p.top) {
          slice = p.list.slice(p.bottom, p.top);
        } else {
          slice = p.list.slice(p.bottom).concat(p.list.slice(0, p.top));
        }
        const length = Math.min(n, slice.length);
        TypedArrayPrototypeSet(ret, slice.slice(0, length), i);
        i += length;
        n -= length;
        if (!p.next) {
          break;
        }
        p = p.next;
      }
    }
    this.length -= ret.length;
    if (hasStrings) {
      ret = ret.join('');
    }
    return ret;
  }
  *[SymbolIterator]() {
    for (let p = this.tail; p !== null; p = p.next) {
      for (let i = p.bottom; i !== p.top; i = (i + 1) & kMask) {
        yield p.list[i];
      }
    }
  }
  first() {
    return this.tail.first();
  }
  shift() {
    const tail = this.tail;
    const next = tail.shift();
    if (tail.isEmpty() && tail.next !== null) {
      // If there is another queue, it forms the new tail.
      this.tail = tail.next;
      tail.next = null;
    }
    this.length -= next?.length;
    return next;
  }
};
