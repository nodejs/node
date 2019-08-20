// META: global=worker
// META: script=resources/readable-stream-from-array.js
// META: script=resources/readable-stream-to-array.js

'use strict';

const inputBytes = [229];

promise_test(async () => {
  const input = readableStreamFromArray([new Uint8Array(inputBytes)]);
  const output = input.pipeThrough(new TextDecoderStream());
  const array = await readableStreamToArray(output);
  assert_array_equals(array, ['\uFFFD'], 'array should have one element');
}, 'incomplete input with error mode "replacement" should end with a ' +
   'replacement character');

promise_test(async t => {
  const input = readableStreamFromArray([new Uint8Array(inputBytes)]);
  const output = input.pipeThrough(new TextDecoderStream(
      'utf-8', {fatal: true}));
  const reader = output.getReader();
  await promise_rejects(t, new TypeError(), reader.read(),
                        'read should reject');
}, 'incomplete input with error mode "fatal" should error the stream');
