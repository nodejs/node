'use strict';
const common = require('../common');
const assert = require('assert');
const Readable = require('stream').Readable;

const readable = new Readable({
  read: () => {}
});

// Initialized to false.
assert.strictEqual(readable._readableState.emittedReadable, false);

const expected = [Buffer.from('foobar'), Buffer.from('quo'), null];
readable.on('readable', common.mustCall(() => {
  // emittedReadable should be true when the readable event is emitted
  assert.strictEqual(readable._readableState.emittedReadable, true);
  assert.deepStrictEqual(readable.read(), expected.shift());
  // emittedReadable is reset to false during read()
  assert.strictEqual(readable._readableState.emittedReadable, false);
}, 3));

// When the first readable listener is just attached,
// emittedReadable should be false
assert.strictEqual(readable._readableState.emittedReadable, false);

// These trigger a single 'readable', as things are batched up
process.nextTick(common.mustCall(() => {
  readable.push('foo');
}));
process.nextTick(common.mustCall(() => {
  readable.push('bar');
}));

// these triggers two readable events
setImmediate(common.mustCall(() => {
  readable.push('quo');
  process.nextTick(common.mustCall(() => {
    readable.push(null);
  }));
}));

const noRead = new Readable({
  read: () => {}
});

noRead.on('readable', common.mustCall(() => {
  // emittedReadable should be true when the readable event is emitted
  assert.strictEqual(noRead._readableState.emittedReadable, true);
  noRead.read(0);
  // emittedReadable is not reset during read(0)
  assert.strictEqual(noRead._readableState.emittedReadable, true);
}));

noRead.push('foo');
noRead.push(null);

const flowing = new Readable({
  read: () => {}
});

flowing.on('data', common.mustCall(() => {
  // When in flowing mode, emittedReadable is always false.
  assert.strictEqual(flowing._readableState.emittedReadable, false);
  flowing.read();
  assert.strictEqual(flowing._readableState.emittedReadable, false);
}, 3));

flowing.push('foooo');
flowing.push('bar');
flowing.push('quo');
process.nextTick(common.mustCall(() => {
  flowing.push(null);
}));
