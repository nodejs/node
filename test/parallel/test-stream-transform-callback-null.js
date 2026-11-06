'use strict';

const common = require('../common');
const assert = require('assert');
const { Transform } = require('stream');

// Passing null as the second argument to the transform callback should be
// equivalent to calling this.push(null), signaling the end of the readable
// side. Refs: https://github.com/nodejs/node/issues/62769

{
  // Calling callback(null, null) should end the readable side of the transform stream.
  const t = new Transform({
    transform(chunk, encoding, callback) {
      callback(null, null);
    },
  });

  t.on('end', common.mustCall());
  t.on('data', (chunk) => {
    // Null sentinel should not appear as a data chunk.
    assert.fail('unexpected data event');
  });

  t.write('hello');
  t.end();
}

{
  // Verify callback(null, data) still works normally.
  const t = new Transform({
    transform(chunk, encoding, callback) {
      callback(null, chunk);
    },
  });

  const received = [];
  t.on('data', (chunk) => received.push(chunk.toString()));
  t.on('end', common.mustCall(() => {
    assert.deepStrictEqual(received, ['hello']);
  }));

  t.write('hello');
  t.end();
}

{
  // Verify callback() with no second arg still works (no push).
  const t = new Transform({
    transform(chunk, encoding, callback) {
      callback();
    },
    flush(callback) {
      callback(null, 'flushed');
    },
  });

  const received = [];
  t.on('data', (chunk) => received.push(chunk.toString()));
  t.on('end', common.mustCall(() => {
    assert.deepStrictEqual(received, ['flushed']);
  }));

  t.write('hello');
  t.end();
}
