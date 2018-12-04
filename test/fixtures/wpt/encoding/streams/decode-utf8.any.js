// META: global=worker
// META: script=resources/readable-stream-from-array.js
// META: script=resources/readable-stream-to-array.js

'use strict';

const emptyChunk = new Uint8Array([]);
const inputChunk = new Uint8Array([73, 32, 240, 159, 146, 153, 32, 115, 116,
                                   114, 101, 97, 109, 115]);
const expectedOutputString = 'I \u{1F499} streams';

promise_test(async () => {
  const input = readableStreamFromArray([inputChunk]);
  const output = input.pipeThrough(new TextDecoderStream());
  const array = await readableStreamToArray(output);
  assert_array_equals(array, [expectedOutputString],
                      'the output should be in one chunk');
}, 'decoding one UTF-8 chunk should give one output string');

promise_test(async () => {
  const input = readableStreamFromArray([emptyChunk]);
  const output = input.pipeThrough(new TextDecoderStream());
  const array = await readableStreamToArray(output);
  assert_array_equals(array, [], 'no chunks should be output');
}, 'decoding an empty chunk should give no output chunks');

promise_test(async () => {
  const input = readableStreamFromArray([emptyChunk, inputChunk]);
  const output = input.pipeThrough(new TextDecoderStream());
  const array = await readableStreamToArray(output);
  assert_array_equals(array, [expectedOutputString],
                      'the output should be in one chunk');
}, 'an initial empty chunk should be ignored');

promise_test(async () => {
  const input = readableStreamFromArray([inputChunk, emptyChunk]);
  const output = input.pipeThrough(new TextDecoderStream());
  const array = await readableStreamToArray(output);
  assert_array_equals(array, [expectedOutputString],
                      'the output should be in one chunk');
}, 'a trailing empty chunk should be ignored');
