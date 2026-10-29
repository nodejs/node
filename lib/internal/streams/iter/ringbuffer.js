'use strict';

// RingBuffer - O(1) FIFO queue with indexed access.
//
// Replaces plain JS arrays that are used as queues with shift()/push().
// Array.shift() is O(n) because it copies all remaining elements;
// RingBuffer.shift() is O(1) -- it just advances a head pointer.
//
// Also provides O(1) trimFront(count) to replace Array.splice(0, count).
//
// Capacity is always a power of 2, so modulo is replaced with bitwise AND.

const {
  Array,
} = primordials;

class RingBuffer {
  #backing;
  #head = 0;
  #size = 0;
  #mask;

  constructor(initialCapacity = 16) {
    this.#mask = initialCapacity - 1;
    this.#backing = new Array(initialCapacity);
  }

  get length() {
    return this.#size;
  }

  /**
   * Append an item to the tail. O(1) amortized.
   */
  push(item) {
    if (this.#size > this.#mask) {
      this.#grow();
    }
    this.#backing[(this.#head + this.#size) & this.#mask] = item;
    this.#size++;
  }

  /**
   * Prepend an item to the head. O(1) amortized.
   */
  unshift(item) {
    if (this.#size > this.#mask) {
      this.#grow();
    }
    this.#head = (this.#head - 1 + this.#mask + 1) & this.#mask;
    this.#backing[this.#head] = item;
    this.#size++;
  }

  /**
   * Remove and return the item at the head. O(1).
   * @returns {any}
   */
  shift() {
    if (this.#size === 0) return undefined;
    const item = this.#backing[this.#head];
    this.#backing[this.#head] = undefined; // Help GC
    this.#head = (this.#head + 1) & this.#mask;
    this.#size--;
    return item;
  }

  /**
   * Read item at a logical index (0 = head). O(1).
   * Returns undefined if index is out of bounds.
   * @returns {any}
   */
  get(index) {
    if (index < 0 || index >= this.#size) return undefined;
    return this.#backing[(this.#head + index) & this.#mask];
  }

  /**
   * Remove `count` items from the head without returning them.
   * O(count) for GC cleanup.
   */
  trimFront(count) {
    if (count <= 0) return;
    if (count >= this.#size) {
      this.clear();
      return;
    }
    for (let i = 0; i < count; i++) {
      this.#backing[(this.#head + i) & this.#mask] = undefined;
    }
    this.#head = (this.#head + count) & this.#mask;
    this.#size -= count;
  }

  /**
   * Find the logical index of `item` (reference equality). O(n).
   * Returns -1 if not found.
   * @returns {number}
   */
  indexOf(item) {
    for (let i = 0; i < this.#size; i++) {
      if (this.#backing[(this.#head + i) & this.#mask] === item) {
        return i;
      }
    }
    return -1;
  }

  /**
   * Remove the item at logical `index`, shifting later elements. O(n) worst case.
   * Used only on rare abort-signal cancellation path.
   */
  removeAt(index) {
    if (index < 0 || index >= this.#size) return;
    for (let i = index; i < this.#size - 1; i++) {
      const from = (this.#head + i + 1) & this.#mask;
      const to = (this.#head + i) & this.#mask;
      this.#backing[to] = this.#backing[from];
    }
    const last = (this.#head + this.#size - 1) & this.#mask;
    this.#backing[last] = undefined;
    this.#size--;
  }

  /**
   * Remove all items. O(n) for GC cleanup.
   */
  clear() {
    for (let i = 0; i < this.#size; i++) {
      this.#backing[(this.#head + i) & this.#mask] = undefined;
    }
    this.#head = 0;
    this.#size = 0;
  }

  /**
   * Double the backing capacity, linearizing the circular layout.
   */
  #grow() {
    const newCapacity = (this.#mask + 1) * 2;
    const newBacking = new Array(newCapacity);
    for (let i = 0; i < this.#size; i++) {
      newBacking[i] = this.#backing[(this.#head + i) & this.#mask];
    }
    this.#backing = newBacking;
    this.#head = 0;
    this.#mask = newCapacity - 1;
  }
}

module.exports = { RingBuffer };
