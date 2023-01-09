'use strict';
const common = require('../common');
const assert = require('assert');
const { pipeline } = require('node:stream/promises');

{
  // Ensure that async iterators can act as readable and writable streams
  async function* myCustomReadable() {
    yield 'Hello';
    yield 'World';
  }

  // eslint-disable-next-line require-yield
  async function* myCustomWritable(stream) {
    const messages = [];
    for await (const chunk of stream) {
      messages.push(chunk);
    }
    assert.deepStrictEqual(messages, ['Hello', 'World']);
  }

  pipeline(
    myCustomReadable,
    myCustomWritable,
  )
  .then(common.mustCall());
}
