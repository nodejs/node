// Flags: --expose-internals
'use strict';
require('../common');
const assert = require('assert');
const BufferList = require('internal/streams/buffer_list');

// Test empty buffer list.
const emptyList = new BufferList();

emptyList.shift();
assert.deepStrictEqual(emptyList, new BufferList());

assert.strictEqual(emptyList.join(','), '');

assert.deepStrictEqual(emptyList.concat(0), Buffer.alloc(0));

const buf = Buffer.from('foo');

function testIterator(list, count) {
  // test iterator
  let len = 0;
  // eslint-disable-next-line no-unused-vars
  for (const x of list) {
    len++;
  }
  assert.strictEqual(len, count);
}

// Test buffer list with one element.
const list = new BufferList();
testIterator(list, 0);

list.push(buf);
testIterator(list, 1);
for (const x of list) {
  assert.strictEqual(x, buf);
}

const copy = list.concat(3);
testIterator(copy, 3);

assert.notStrictEqual(copy, buf);
assert.deepStrictEqual(copy, buf);

assert.strictEqual(list.join(','), 'foo');

const shifted = list.shift();
testIterator(list, 0);
assert.strictEqual(shifted, buf);
assert.deepStrictEqual(list, new BufferList());

{
  const list = new BufferList();
  list.push('foo');
  list.push('bar');
  list.push('foo');
  list.push('bar');
  assert.strictEqual(list.consume(6, true), 'foobar');
  assert.strictEqual(list.consume(6, true), 'foobar');
}

{
  const list = new BufferList();
  list.push('foo');
  list.push('bar');
  assert.strictEqual(list.consume(5, true), 'fooba');
}

{
  const list = new BufferList();
  list.push(buf);
  list.push(buf);
  list.push(buf);
  list.push(buf);
  assert.strictEqual(list.consume(6).toString(), 'foofoo');
  assert.strictEqual(list.consume(6).toString(), 'foofoo');
}

{
  const list = new BufferList();
  list.push(buf);
  list.push(buf);
  assert.strictEqual(list.consume(5).toString(), 'foofo');
}
