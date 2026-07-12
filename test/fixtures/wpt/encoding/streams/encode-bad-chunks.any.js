// META: global=window,worker
// META: script=resources/readable-stream-from-array.js
// META: script=resources/readable-stream-to-array.js

'use strict';

const error1 = new Error('error1');
error1.name = 'error1';

promise_test(t => {
  const ts = new TextEncoderStream();
  const writer = ts.writable.getWriter();
  const reader = ts.readable.getReader();
  const writePromise = writer.write({
    toString() { throw error1; }
  });
  const readPromise = reader.read();
  return Promise.all([
    promise_rejects_exactly(t, error1, readPromise, 'read should reject with error1'),
    promise_rejects_exactly(t, error1, writePromise, 'write should reject with error1'),
    promise_rejects_exactly(t, error1, reader.closed, 'readable should be errored with error1'),
    promise_rejects_exactly(t, error1, writer.closed, 'writable should be errored with error1'),
  ]);
}, 'a chunk that cannot be converted to a string should error the streams');

const oddInputs = [
  {
    name: 'undefined',
    value: undefined,
    expected: 'undefined'
  },
  {
    name: 'null',
    value: null,
    expected: 'null'
  },
  {
    name: 'numeric',
    value: 3.14,
    expected: '3.14'
  },
  {
    name: 'object',
    value: {},
    expected: '[object Object]'
  },
  {
    name: 'array',
    value: ['hi'],
    expected: 'hi'
  }
];

for (const input of oddInputs) {
  promise_test(async () => {
    const outputReadable = readableStreamFromArray([input.value])
          .pipeThrough(new TextEncoderStream())
          .pipeThrough(new TextDecoderStream());
    const output = await readableStreamToArray(outputReadable);
    assert_equals(output.length, 1, 'output should contain one chunk');
    assert_equals(output[0], input.expected, 'output should be correct');
  }, `input of type ${input.name} should be converted correctly to string`);
}
