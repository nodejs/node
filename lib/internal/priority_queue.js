'use strict';

const {
  Array,
} = primordials;

// The PriorityQueue is a basic implementation of a binary heap that accepts
// a custom sorting function via its constructor. This function is passed
// the two nodes to compare, similar to the native Array#sort. Crucially
// this enables priority queues that are based on a comparison of more than
// just a single criteria.

module.exports = class PriorityQueue {
  #compare = (a, b) => a - b;
  #heap = new Array(64);
  #setPosition;
  #size = 0;

  constructor(comparator, setPosition) {
    if (comparator !== undefined)
      this.#compare = comparator;
    if (setPosition !== undefined)
      this.#setPosition = setPosition;
  }

  insert(value) {
    const heap = this.#heap;
    const pos = ++this.#size;
    heap[pos] = value;

    if (heap.length === pos)
      heap.length *= 2;

    this.percolateUp(pos);
  }

  peek() {
    return this.#heap[1];
  }

  percolateDown(pos) {
    const compare = this.#compare;
    const setPosition = this.#setPosition;
    const heap = this.#heap;
    const size = this.#size;
    const item = heap[pos];

    while (pos * 2 <= size) {
      let childIndex = pos * 2 + 1;
      if (childIndex > size || compare(heap[pos * 2], heap[childIndex]) < 0)
        childIndex = pos * 2;
      const child = heap[childIndex];
      if (compare(item, child) <= 0)
        break;
      if (setPosition !== undefined)
        setPosition(child, pos);
      heap[pos] = child;
      pos = childIndex;
    }
    heap[pos] = item;
    if (setPosition !== undefined)
      setPosition(item, pos);
  }

  percolateUp(pos) {
    const heap = this.#heap;
    const compare = this.#compare;
    const setPosition = this.#setPosition;
    const item = heap[pos];

    while (pos > 1) {
      const parent = heap[pos / 2 | 0];
      if (compare(parent, item) <= 0)
        break;
      heap[pos] = parent;
      if (setPosition !== undefined)
        setPosition(parent, pos);
      pos = pos / 2 | 0;
    }
    heap[pos] = item;
    if (setPosition !== undefined)
      setPosition(item, pos);
  }

  removeAt(pos) {
    const heap = this.#heap;
    const size = --this.#size;
    heap[pos] = heap[size + 1];
    heap[size + 1] = undefined;

    if (size > 0 && pos <= size) {
      if (pos > 1 && this.#compare(heap[pos / 2 | 0], heap[pos]) > 0)
        this.percolateUp(pos);
      else
        this.percolateDown(pos);
    }
  }

  shift() {
    const heap = this.#heap;
    const value = heap[1];
    if (value === undefined)
      return;

    this.removeAt(1);

    return value;
  }
};
