// META: global=window,worker
// META: script=resources/readable-stream-from-array.js
// META: script=resources/readable-stream-to-array.js

'use strict';

const inputBytes = [73, 32, 240, 159, 146, 153, 32, 115, 116, 114, 101,
                    97, 109, 115];
for (const splitPoint of [2, 3, 4, 5]) {
  promise_test(async () => {
    const input = readableStreamFromArray(
        [new Uint8Array(inputBytes.slice(0, splitPoint)),
         new Uint8Array(inputBytes.slice(splitPoint))]);
    const expectedOutput = ['I ', '\u{1F499} streams'];
    const output = input.pipeThrough(new TextDecoderStream());
    const array = await readableStreamToArray(output);
    assert_array_equals(array, expectedOutput,
                        'the split code point should be in the second chunk ' +
                        'of the output');
  }, 'a code point split between chunks should not be emitted until all ' +
      'bytes are available; split point = ' + splitPoint);
}

promise_test(async () => {
  const splitPoint = 6;
  const input = readableStreamFromArray(
      [new Uint8Array(inputBytes.slice(0, splitPoint)),
       new Uint8Array(inputBytes.slice(splitPoint))]);
  const output = input.pipeThrough(new TextDecoderStream());
  const array = await readableStreamToArray(output);
  assert_array_equals(array, ['I \u{1F499}', ' streams'],
                      'the multibyte character should be in the first chunk ' +
                      'of the output');
}, 'a code point should be emitted as soon as all bytes are available');

for (let splitPoint = 1; splitPoint < 7; ++splitPoint) {
  promise_test(async () => {
    const input = readableStreamFromArray(
      [new Uint8Array(inputBytes.slice(0, splitPoint)),
       new Uint8Array([]),
       new Uint8Array(inputBytes.slice(splitPoint))]);
    const concatenatedOutput = 'I \u{1F499} streams';
    const output = input.pipeThrough(new TextDecoderStream());
    const array = await readableStreamToArray(output);
    assert_equals(array.length, 2, 'two chunks should be output');
    assert_equals(array[0].concat(array[1]), concatenatedOutput,
                  'output should be unchanged by the empty chunk');
  }, 'an empty chunk inside a code point split between chunks should not ' +
     'change the output; split point = ' + splitPoint);
}
