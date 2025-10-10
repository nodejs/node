'use strict';

const common = require('../common');
const assert = require('assert');
const PriorityQueue = require('internal/priority_queue_adaptive');

// Basic functionality tests
{
  const queue = new PriorityQueue();

  assert.strictEqual(queue.size, 0);
  assert.strictEqual(queue.isEmpty(), true);
  assert.strictEqual(queue.peek(), undefined);
  assert.strictEqual(queue.shift(), undefined);
}

// Insert and peek in linear mode (n < 10)
{
  const queue = new PriorityQueue();

  queue.insert(5);
  assert.strictEqual(queue.peek(), 5);
  assert.strictEqual(queue.size, 1);

  queue.insert(3);
  assert.strictEqual(queue.peek(), 3);

  queue.insert(7);
  assert.strictEqual(queue.peek(), 3);

  queue.insert(1);
  assert.strictEqual(queue.peek(), 1);
  assert.strictEqual(queue.size, 4);
}

// Shift in linear mode
{
  const queue = new PriorityQueue();

  queue.insert(5);
  queue.insert(3);
  queue.insert(7);
  queue.insert(1);

  assert.strictEqual(queue.shift(), 1);
  assert.strictEqual(queue.shift(), 3);
  assert.strictEqual(queue.shift(), 5);
  assert.strictEqual(queue.shift(), 7);
  assert.strictEqual(queue.shift(), undefined);
  assert.strictEqual(queue.size, 0);
}

// Custom comparator
{
  const queue = new PriorityQueue((a, b) => b - a); // Max heap

  queue.insert(5);
  queue.insert(3);
  queue.insert(7);
  queue.insert(1);

  assert.strictEqual(queue.peek(), 7);
  assert.strictEqual(queue.shift(), 7);
  assert.strictEqual(queue.shift(), 5);
  assert.strictEqual(queue.shift(), 3);
  assert.strictEqual(queue.shift(), 1);
}

// SetPosition callback
{
  const items = [];
  const setPosition = (item, pos) => {
    item.pos = pos;
  };

  const queue = new PriorityQueue((a, b) => a.value - b.value, setPosition);

  const item1 = { value: 5, pos: null };
  const item2 = { value: 3, pos: null };
  const item3 = { value: 7, pos: null };

  queue.insert(item1);
  queue.insert(item2);
  queue.insert(item3);

  // In linear mode, min item position is tracked internally
  // but may not be at index 0 since array is unsorted
  assert.ok(item2.pos >= 0 && item2.pos < 3);
  assert.strictEqual(queue.peek(), item2);
}

// Mode switching: linear -> heap at n=10
{
  const queue = new PriorityQueue();

  // Insert 9 items (should stay in linear mode)
  for (let i = 9; i >= 1; i--) {
    queue.insert(i);
  }
  assert.strictEqual(queue.size, 9);
  assert.strictEqual(queue.peek(), 1);

  // Insert 10th item (should trigger conversion to heap mode)
  queue.insert(0);
  assert.strictEqual(queue.size, 10);
  assert.strictEqual(queue.peek(), 0);

  // Verify heap mode works correctly
  for (let i = 0; i < 10; i++) {
    assert.strictEqual(queue.shift(), i);
  }
}

// Mode switching: heap -> linear at n=6 (hysteresis)
{
  const queue = new PriorityQueue();

  // Fill to 10 items (heap mode)
  for (let i = 0; i < 10; i++) {
    queue.insert(i);
  }
  assert.strictEqual(queue.size, 10);

  // Shift down to 9 (should stay in heap mode due to hysteresis)
  queue.shift();
  assert.strictEqual(queue.size, 9);

  // Shift down to 8, 7, 6 (should still be heap mode)
  queue.shift();
  queue.shift();
  queue.shift();
  assert.strictEqual(queue.size, 6);

  // Shift to 5 (should convert back to linear mode)
  queue.shift();
  assert.strictEqual(queue.size, 5);

  // Verify linear mode works correctly
  assert.strictEqual(queue.shift(), 5);
  assert.strictEqual(queue.shift(), 6);
  assert.strictEqual(queue.shift(), 7);
  assert.strictEqual(queue.shift(), 8);
  assert.strictEqual(queue.shift(), 9);
}

// No thrashing: oscillating around threshold
{
  const queue = new PriorityQueue();

  // Fill to 9 items
  for (let i = 0; i < 9; i++) {
    queue.insert(i);
  }

  // Oscillate: insert to 10, shift to 9, repeat
  // Should NOT thrash due to hysteresis (up=10, down=6)
  for (let i = 0; i < 100; i++) {
    queue.insert(100 + i);  // n=10 (heap mode)
    assert.strictEqual(queue.size, 10);
    queue.shift();          // n=9 (should stay heap mode)
    assert.strictEqual(queue.size, 9);
  }

  // Queue should still work correctly
  while (queue.size > 0) {
    queue.shift();
  }
  assert.strictEqual(queue.size, 0);
}

// RemoveAt in linear mode
{
  const items = [];
  const setPosition = (item, pos) => {
    item.pos = pos;
  };

  const queue = new PriorityQueue((a, b) => a.value - b.value, setPosition);

  for (let i = 0; i < 5; i++) {
    const item = { value: i, pos: null };
    items.push(item);
    queue.insert(item);
  }

  // Remove item at position 2
  const itemToRemove = items[2];
  queue.removeAt(itemToRemove.pos);
  assert.strictEqual(queue.size, 4);

  // Verify remaining items
  assert.strictEqual(queue.shift().value, 0);
  assert.strictEqual(queue.shift().value, 1);
  assert.strictEqual(queue.shift().value, 3);
  assert.strictEqual(queue.shift().value, 4);
}

// RemoveAt in heap mode
{
  const items = [];
  const setPosition = (item, pos) => {
    item.pos = pos;
  };

  const queue = new PriorityQueue((a, b) => a.value - b.value, setPosition);

  for (let i = 0; i < 15; i++) {
    const item = { value: i, pos: null };
    items.push(item);
    queue.insert(item);
  }

  // Remove item at specific position
  const itemToRemove = items[7];
  queue.removeAt(itemToRemove.pos);
  assert.strictEqual(queue.size, 14);

  // Verify min is still correct
  assert.strictEqual(queue.peek().value, 0);
}

// Heapify with small array (linear mode)
{
  const queue = new PriorityQueue();
  const array = [5, 3, 7, 1, 9];

  queue.heapify(array);
  assert.strictEqual(queue.size, 5);
  assert.strictEqual(queue.peek(), 1);

  assert.strictEqual(queue.shift(), 1);
  assert.strictEqual(queue.shift(), 3);
  assert.strictEqual(queue.shift(), 5);
  assert.strictEqual(queue.shift(), 7);
  assert.strictEqual(queue.shift(), 9);
}

// Heapify with large array (heap mode)
{
  const queue = new PriorityQueue();
  const array = [50, 30, 70, 10, 90, 20, 80, 40, 60, 100, 5, 15];

  queue.heapify(array);
  assert.strictEqual(queue.size, 12);
  assert.strictEqual(queue.peek(), 5);

  // Verify sorted order
  let prev = queue.shift();
  while (queue.size > 0) {
    const curr = queue.shift();
    assert.ok(curr >= prev, `Expected ${curr} >= ${prev}`);
    prev = curr;
  }
}

// PeekBottom
{
  const queue = new PriorityQueue();

  queue.insert(5);
  queue.insert(3);
  queue.insert(7);

  // peekBottom returns last element in array (not necessarily max)
  const bottom = queue.peekBottom();
  assert.ok(bottom === 5 || bottom === 3 || bottom === 7);
}

// Clear
{
  const queue = new PriorityQueue();

  for (let i = 0; i < 10; i++) {
    queue.insert(i);
  }

  queue.clear();
  assert.strictEqual(queue.size, 0);
  assert.strictEqual(queue.isEmpty(), true);
  assert.strictEqual(queue.peek(), undefined);
}

// Legacy API: percolateDown with 1-indexing
{
  const items = [];
  const setPosition = (item, pos) => {
    item.pos = pos;
  };

  const queue = new PriorityQueue((a, b) => a.value - b.value, setPosition);

  for (let i = 0; i < 15; i++) {
    const item = { value: i, pos: null };
    items.push(item);
    queue.insert(item);
  }

  // Modify min item and percolate down (using 1-indexed API)
  const min = queue.peek();
  min.value = 100;
  queue.percolateDown(1); // Should convert 1 -> 0

  // Min should now be different
  assert.notStrictEqual(queue.peek(), min);
  assert.ok(queue.peek().value < 100);
}

// Legacy API: percolateUp with 1-indexing
{
  const items = [];
  const setPosition = (item, pos) => {
    item.pos = pos;
  };

  const queue = new PriorityQueue((a, b) => a.value - b.value, setPosition);

  for (let i = 0; i < 15; i++) {
    const item = { value: i, pos: null };
    items.push(item);
    queue.insert(item);
  }

  // Find a non-min item, decrease its value, and percolate up
  const item = items[10];
  const originalValue = item.value;
  item.value = -1;
  queue.percolateUp(item.pos === 0 ? 1 : item.pos); // Handle 1-indexing

  // This item might now be the min
  const newMin = queue.peek();
  assert.ok(newMin.value <= originalValue);
}

// Stress test: large number of operations
{
  const queue = new PriorityQueue();
  const values = [];

  // Insert 1000 random values
  for (let i = 0; i < 1000; i++) {
    const value = Math.floor(Math.random() * 10000);
    queue.insert(value);
    values.push(value);
  }

  values.sort((a, b) => a - b);

  // Verify sorted order
  for (let i = 0; i < 1000; i++) {
    assert.strictEqual(queue.shift(), values[i]);
  }
}

// DOS attack pattern: thrashing attempt
{
  const queue = new PriorityQueue();

  // Fill to threshold - 1
  for (let i = 0; i < 9; i++) {
    queue.insert(i);
  }

  const startTime = Date.now();

  // Attempt to cause thrashing by oscillating around threshold
  for (let i = 0; i < 10000; i++) {
    queue.insert(1000 + i);  // n=10
    queue.shift();           // n=9
  }

  const endTime = Date.now();
  const duration = endTime - startTime;

  // Should complete in reasonable time (not degrade to O(n²))
  // On typical hardware, this should take < 100ms
  assert.ok(duration < 1000, `DOS pattern took ${duration}ms (expected < 1000ms)`);

  // Queue should still be functional
  assert.strictEqual(queue.size, 9);
}

console.log('All tests passed!');
