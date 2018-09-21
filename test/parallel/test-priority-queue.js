// Flags: --expose-internals
'use strict';

require('../common');

const assert = require('assert');
const PriorityQueue = require('internal/priority_queue');

{
  // Checks that the queue is fundamentally correct.
  const queue = new PriorityQueue();
  for (let i = 15; i > 0; i--)
    queue.insert(i);

  for (let i = 1; i < 16; i++) {
    assert.strictEqual(queue.peek(), i);
    assert.strictEqual(queue.shift(), i);
  }

  assert.strictEqual(queue.shift(), undefined);

  // Reverse the order.
  for (let i = 1; i < 16; i++)
    queue.insert(i);

  for (let i = 1; i < 16; i++) {
    assert.strictEqual(queue.shift(), i);
  }

  assert.strictEqual(queue.shift(), undefined);
}

{
  // Checks that the queue is capable of resizing and fitting more elements.
  const queue = new PriorityQueue();
  for (let i = 2048; i > 0; i--)
    queue.insert(i);

  for (let i = 1; i < 2049; i++) {
    assert.strictEqual(queue.shift(), i);
  }

  assert.strictEqual(queue.shift(), undefined);
}

{
  // Checks that remove works as expected.
  const queue = new PriorityQueue();
  for (let i = 16; i > 0; i--)
    queue.insert(i);

  const removed = [5, 10, 15];
  for (const id of removed)
    assert(queue.remove(id));

  assert(!queue.remove(100));
  assert(!queue.remove(-100));

  for (let i = 1; i < 17; i++) {
    if (removed.indexOf(i) < 0)
      assert.strictEqual(queue.shift(), i);
  }

  assert.strictEqual(queue.shift(), undefined);
}

{
  // Make a max heap with a custom sort function.
  const queue = new PriorityQueue((a, b) => b - a);
  for (let i = 1; i < 17; i++)
    queue.insert(i);

  for (let i = 16; i > 0; i--) {
    assert.strictEqual(queue.shift(), i);
  }

  assert.strictEqual(queue.shift(), undefined);
}

{
  // Make a min heap that accepts objects as values, which necessitates
  // a custom sorting function. In addition, add a setPosition function
  // as 2nd param which provides a reference to the node in the heap
  // and allows speedy deletions.
  const queue = new PriorityQueue((a, b) => {
    return a.value - b.value;
  }, (node, pos) => (node.position = pos));
  for (let i = 1; i < 17; i++)
    queue.insert({ value: i, position: null });

  for (let i = 1; i < 17; i++) {
    assert.strictEqual(queue.peek().value, i);
    queue.removeAt(queue.peek().position);
  }

  assert.strictEqual(queue.peek(), undefined);
}
