'use strict';

// The PriorityQueue is a basic implementation of a binary heap that accepts
// a custom sorting function via its constructor. This function is passed
// the two nodes to compare, similar to the native Array#sort. Crucially
// this enables priority queues that are based on a comparison of more than
// just a single criteria.

module.exports = class PriorityQueue {
  #compare = (a, b) => a - b;
  #heap = [undefined, undefined];
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

    this.percolateUp(pos);
  }

  peek() {
    return this.#heap[1];
  }

  peekBottom() {
    return this.#heap[this.#size];
  }

  percolateDown(pos) {
    const compare = this.#compare;
    const setPosition = this.#setPosition;
    const hasSetPosition = setPosition !== undefined;
    const heap = this.#heap;
    const size = this.#size;
    const hsize = size >> 1;
    const item = heap[pos];

    while (pos <= hsize) {
      let child = pos << 1;
      const nextChild = child + 1;
      let childItem = heap[child];

      if (nextChild <= size && compare(heap[nextChild], childItem) < 0) {
        child = nextChild;
        childItem = heap[nextChild];
      }

      if (compare(item, childItem) <= 0) break;

      if (hasSetPosition)
        setPosition(childItem, pos);

      heap[pos] = childItem;
      pos = child;
    }

    heap[pos] = item;
    if (hasSetPosition)
      setPosition(item, pos);
  }

  percolateUp(pos) {
    const heap = this.#heap;
    const compare = this.#compare;
    const setPosition = this.#setPosition;
    const hasSetPosition = setPosition !== undefined;
    const item = heap[pos];

    while (pos > 1) {
      const parent = pos >> 1;
      const parentItem = heap[parent];
      if (compare(parentItem, item) <= 0)
        break;
      heap[pos] = parentItem;
      if (hasSetPosition)
        setPosition(parentItem, pos);
      pos = parent;
    }

    heap[pos] = item;
    if (hasSetPosition)
      setPosition(item, pos);
  }

  removeAt(pos) {
    const heap = this.#heap;
    let size = this.#size;
    heap[pos] = heap[size];
    heap[size] = undefined;
    size = --this.#size;

    if (size > 0 && pos <= size) {
      if (pos > 1 && this.#compare(heap[pos >> 1], heap[pos]) > 0)
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
