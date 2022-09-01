// Flags: --expose-internals
'use strict';

require('../common');

const assert = require('assert');
const FixedQueue = require('internal/fixed_queue');

{
  const queue = new FixedQueue();
  assert.strictEqual(queue.head, queue.tail);
  assert(queue.isEmpty());
  queue.push('a');
  assert(!queue.isEmpty());
  assert.strictEqual(queue.shift(), 'a');
  assert.strictEqual(queue.head, queue.tail);
  assert.strictEqual(queue.head.next, null);
  assert.strictEqual(queue.shift(), null);
  assert.strictEqual(queue.head, queue.tail);
  assert.strictEqual(queue.head.next, null);
}

{
  const queue = new FixedQueue();
  for (let i = 0; i < 2047; i++)
    queue.push('a');
  assert(queue.head.isFull());
  queue.push('a');
  assert(!queue.head.isFull());
  assert.notStrictEqual(queue.head, queue.tail);
  assert.strictEqual(queue.head, queue.tail.next);
  assert.strictEqual(queue.head.next, null);

  for (let i = 0; i < 2047; i++)
    assert.strictEqual(queue.shift(), 'a');
  assert.strictEqual(queue.head, queue.tail);
  assert.strictEqual(queue.head.next, null);
  assert(!queue.isEmpty());
  assert.strictEqual(queue.shift(), 'a');
  assert(queue.isEmpty());
}
