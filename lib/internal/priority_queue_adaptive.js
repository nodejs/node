'use strict';

// Adaptive Priority Queue for Node.js Timers
//
// This implementation uses different strategies based on queue size to optimize
// for the typical case while handling pathological cases gracefully:
//
// - Small queues (n < 10): Unsorted array with cached minimum position
//   - O(1) peek, O(1) insert, O(n) shift
//   - Optimal for typical Node.js timer usage where n = 3-20
//
// - Large queues (n >= 10): 4-ary heap
//   - O(1) peek, O(log₄ n) insert/shift
//   - Handles pathological cases (DOS attacks, unusual workloads)
//
// Transitions between modes happen automatically with minimal overhead.
// This design is informed by real-world Node.js timer patterns where the queue
// holds TimersList objects (one per unique timeout duration), not individual
// timers, resulting in small queue sizes even in large applications.

// Hysteresis thresholds to prevent mode switching thrashing
const HEAP_THRESHOLD = 10;      // Switch to heap when size reaches this
const LINEAR_THRESHOLD = 6;     // Switch back to linear when size drops to this
const D = 4; // D-ary branching factor

module.exports = class PriorityQueueAdaptive {
  #compare = (a, b) => a - b;
  #heap = [];
  #setPosition = undefined;
  #size = 0;
  #minPos = 0;  // For small mode
  #isHeap = false;  // false = linear scan mode, true = heap mode

  constructor(comparator, setPosition) {
    if (comparator !== undefined)
      this.#compare = comparator;
    if (setPosition !== undefined)
      this.#setPosition = setPosition;
  }

  peek() {
    if (this.#size === 0) return undefined;
    return this.#isHeap ? this.#heap[0] : this.#heap[this.#minPos];
  }

  peekBottom() {
    const size = this.#size;
    return size > 0 ? this.#heap[size - 1] : undefined;
  }

  insert(value) {
    const size = this.#size;

    // Check if we need to convert to heap mode
    if (!this.#isHeap && size >= HEAP_THRESHOLD) {
      this.#convertToHeap();
    }

    if (this.#isHeap) {
      this.#heapInsert(value);
    } else {
      this.#linearInsert(value);
    }
  }

  shift() {
    if (this.#size === 0) return undefined;

    const result = this.#isHeap ? this.#heapShift() : this.#linearShift();

    // Convert back to linear if size drops below lower threshold (hysteresis)
    if (this.#isHeap && this.#size < LINEAR_THRESHOLD) {
      this.#convertToLinear();
    }

    return result;
  }

  removeAt(pos) {
    if (pos >= this.#size) return;

    if (this.#isHeap) {
      this.#heapRemoveAt(pos);
    } else {
      this.#linearRemoveAt(pos);
    }

    if (this.#isHeap && this.#size < LINEAR_THRESHOLD) {
      this.#convertToLinear();
    }
  }

  heapify(array) {
    const len = array.length;
    this.#size = len;
    const heap = this.#heap;

    for (let i = 0; i < len; i++) {
      heap[i] = array[i];
    }

    if (len >= HEAP_THRESHOLD) {
      this.#isHeap = true;
      this.#heapifyArray();
    } else {
      this.#isHeap = false;
      this.#updateMinPosition();

      if (this.#setPosition !== undefined) {
        for (let i = 0; i < len; i++) {
          this.#setPosition(heap[i], i);
        }
      }
    }
  }

  get size() {
    return this.#size;
  }

  isEmpty() {
    return this.#size === 0;
  }

  clear() {
    const heap = this.#heap;
    const size = this.#size;
    for (let i = 0; i < size; i++) {
      heap[i] = undefined;
    }
    this.#size = 0;
    this.#minPos = 0;
    this.#isHeap = false;
  }

  _validate() {
    if (this.#isHeap) {
      return this.#validateHeap();
    } else {
      return this.#validateLinear();
    }
  }

  // LINEAR MODE METHODS
  #linearInsert(value) {
    const heap = this.#heap;
    const pos = this.#size++;
    heap[pos] = value;

    if (this.#setPosition !== undefined)
      this.#setPosition(value, pos);

    if (pos === 0 || this.#compare(value, heap[this.#minPos]) < 0) {
      this.#minPos = pos;
    }
  }

  #linearShift() {
    const heap = this.#heap;
    const minPos = this.#minPos;
    const minValue = heap[minPos];
    const lastPos = this.#size - 1;

    // If removing the last element, just clear it
    if (lastPos === 0) {
      heap[0] = undefined;
      this.#size = 0;
      this.#minPos = 0;
      return minValue;
    }

    // Swap min with last element
    const lastValue = heap[lastPos];
    heap[minPos] = lastValue;
    heap[lastPos] = undefined;
    this.#size = lastPos;

    if (this.#setPosition !== undefined)
      this.#setPosition(lastValue, minPos);

    // Find new minimum
    this.#updateMinPosition();
    return minValue;
  }

  #linearRemoveAt(pos) {
    const heap = this.#heap;
    const lastPos = this.#size - 1;

    if (pos === lastPos) {
      heap[lastPos] = undefined;
      this.#size = lastPos;
      if (this.#minPos === lastPos) {
        this.#updateMinPosition();
      }
      return;
    }

    const lastValue = heap[lastPos];
    heap[pos] = lastValue;
    heap[lastPos] = undefined;
    this.#size = lastPos;

    if (this.#setPosition !== undefined)
      this.#setPosition(lastValue, pos);

    if (pos === this.#minPos) {
      this.#updateMinPosition();
    } else if (this.#compare(lastValue, heap[this.#minPos]) < 0) {
      this.#minPos = pos;
    }
  }

  #updateMinPosition() {
    const size = this.#size;
    if (size === 0) {
      this.#minPos = 0;
      return;
    }

    const heap = this.#heap;
    const compare = this.#compare;
    let minPos = 0;
    let minValue = heap[0];

    for (let i = 1; i < size; i++) {
      if (compare(heap[i], minValue) < 0) {
        minPos = i;
        minValue = heap[i];
      }
    }

    this.#minPos = minPos;
  }

  #validateLinear() {
    const heap = this.#heap;
    const compare = this.#compare;
    const size = this.#size;
    const minPos = this.#minPos;

    if (size === 0) return true;
    if (minPos >= size) return false;

    const minValue = heap[minPos];
    for (let i = 0; i < size; i++) {
      if (compare(heap[i], minValue) < 0) {
        return false;
      }
    }
    return true;
  }

  // HEAP MODE METHODS (4-ary heap)
  #heapInsert(value) {
    const heap = this.#heap;
    let pos = this.#size++;
    heap[pos] = value;

    if (this.#setPosition !== undefined)
      this.#setPosition(value, pos);

    this.#percolateUp(pos);
  }

  #heapShift() {
    const heap = this.#heap;
    const min = heap[0];
    const lastPos = this.#size - 1;

    if (lastPos === 0) {
      heap[0] = undefined;
      this.#size = 0;
      return min;
    }

    const last = heap[lastPos];
    heap[0] = last;
    heap[lastPos] = undefined;
    this.#size = lastPos;

    if (this.#setPosition !== undefined)
      this.#setPosition(last, 0);

    this.#percolateDown(0);
    return min;
  }

  #heapRemoveAt(pos) {
    const heap = this.#heap;
    const lastPos = this.#size - 1;

    if (pos === lastPos) {
      heap[lastPos] = undefined;
      this.#size = lastPos;
      return;
    }

    const last = heap[lastPos];
    heap[pos] = last;
    heap[lastPos] = undefined;
    this.#size = lastPos;

    if (this.#setPosition !== undefined)
      this.#setPosition(last, pos);

    const compare = this.#compare;
    const parent = Math.floor((pos - 1) / D);
    if (pos > 0 && compare(last, heap[parent]) < 0) {
      this.#percolateUp(pos);
    } else {
      this.#percolateDown(pos);
    }
  }

  #percolateUp(pos) {
    const heap = this.#heap;
    const compare = this.#compare;
    const setPosition = this.#setPosition;
    const item = heap[pos];

    while (pos > 0) {
      const parent = Math.floor((pos - 1) / D);
      const parentItem = heap[parent];

      if (compare(parentItem, item) <= 0) break;

      heap[pos] = parentItem;
      if (setPosition !== undefined)
        setPosition(parentItem, pos);

      pos = parent;
    }

    heap[pos] = item;
    if (setPosition !== undefined)
      setPosition(item, pos);
  }

  #percolateDown(pos) {
    const heap = this.#heap;
    const compare = this.#compare;
    const setPosition = this.#setPosition;
    const size = this.#size;
    const item = heap[pos];
    const lastParent = Math.floor((size - 1 - 1) / D);

    while (pos <= lastParent) {
      const firstChild = pos * D + 1;
      let minChild = firstChild;
      let minChildItem = heap[firstChild];

      for (let i = 1; i < D; i++) {
        const child = firstChild + i;
        if (child >= size) break;

        const childItem = heap[child];
        if (compare(childItem, minChildItem) < 0) {
          minChild = child;
          minChildItem = childItem;
        }
      }

      if (compare(item, minChildItem) <= 0) break;

      heap[pos] = minChildItem;
      if (setPosition !== undefined)
        setPosition(minChildItem, pos);

      pos = minChild;
    }

    heap[pos] = item;
    if (setPosition !== undefined)
      setPosition(item, pos);
  }

  #heapifyArray() {
    const size = this.#size;
    const startPos = Math.floor((size - 1 - 1) / D);

    for (let pos = startPos; pos >= 0; pos--) {
      this.#percolateDown(pos);
    }

    if (this.#setPosition !== undefined) {
      for (let i = 0; i < size; i++) {
        this.#setPosition(this.#heap[i], i);
      }
    }
  }

  #validateHeap() {
    const heap = this.#heap;
    const compare = this.#compare;
    const size = this.#size;

    for (let pos = 0; pos < size; pos++) {
      const firstChild = pos * D + 1;
      for (let i = 0; i < D; i++) {
        const child = firstChild + i;
        if (child < size && compare(heap[child], heap[pos]) < 0) {
          return false;
        }
      }
    }
    return true;
  }

  // CONVERSION METHODS
  #convertToHeap() {
    this.#isHeap = true;
    this.#heapifyArray();
  }

  #convertToLinear() {
    this.#isHeap = false;
    this.#updateMinPosition();
  }

  // LEGACY API
  percolateDown(pos) {
    if (pos === 1) pos = 0;

    if (this.#isHeap) {
      this.#percolateDown(pos);
    } else {
      // In linear mode, just update min if modified
      if (pos === this.#minPos) {
        this.#updateMinPosition();
      }
    }
  }

  percolateUp(pos) {
    if (pos === 1) pos = 0;

    if (this.#isHeap) {
      this.#percolateUp(pos);
    } else {
      const heap = this.#heap;
      if (pos < this.#size && this.#compare(heap[pos], heap[this.#minPos]) < 0) {
        this.#minPos = pos;
      }
    }
  }
};
