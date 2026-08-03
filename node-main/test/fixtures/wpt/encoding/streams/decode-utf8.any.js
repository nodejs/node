// META: global=window,worker
// META: script=resources/readable-stream-from-array.js
// META: script=resources/readable-stream-to-array.js
// META: script=/common/sab.js
'use strict';

["ArrayBuffer", "SharedArrayBuffer"].forEach((arrayBufferOrSharedArrayBuffer) => {
  const inputChunkData = [73, 32, 240, 159, 146, 153, 32, 115, 116, 114, 101, 97, 109, 115];

  const emptyChunk = new Uint8Array(createBuffer(arrayBufferOrSharedArrayBuffer, 0));
  const inputChunk = new Uint8Array(createBuffer(arrayBufferOrSharedArrayBuffer, inputChunkData.length));

  inputChunk.set(inputChunkData);

  const expectedOutputString = 'I \u{1F499} streams';

  promise_test(async () => {
    const input = readableStreamFromArray([inputChunk]);
    const output = input.pipeThrough(new TextDecoderStream());
    const array = await readableStreamToArray(output);
    assert_array_equals(array, [expectedOutputString],
                        'the output should be in one chunk');
  }, 'decoding one UTF-8 chunk should give one output string - ' + arrayBufferOrSharedArrayBuffer);

  promise_test(async () => {
    const input = readableStreamFromArray([emptyChunk]);
    const output = input.pipeThrough(new TextDecoderStream());
    const array = await readableStreamToArray(output);
    assert_array_equals(array, [], 'no chunks should be output');
  }, 'decoding an empty chunk should give no output chunks - ' + arrayBufferOrSharedArrayBuffer);

  promise_test(async () => {
    const input = readableStreamFromArray([emptyChunk, inputChunk]);
    const output = input.pipeThrough(new TextDecoderStream());
    const array = await readableStreamToArray(output);
    assert_array_equals(array, [expectedOutputString],
                        'the output should be in one chunk');
  }, 'an initial empty chunk should be ignored - ' + arrayBufferOrSharedArrayBuffer);

  promise_test(async () => {
    const input = readableStreamFromArray([inputChunk, emptyChunk]);
    const output = input.pipeThrough(new TextDecoderStream());
    const array = await readableStreamToArray(output);
    assert_array_equals(array, [expectedOutputString],
                        'the output should be in one chunk');
  }, 'a trailing empty chunk should be ignored - ' + arrayBufferOrSharedArrayBuffer);

  promise_test(async () => {
    const chunk = new Uint8Array(createBuffer(arrayBufferOrSharedArrayBuffer, 3));
    chunk.set([0xF0, 0x9F, 0x92]);
    const input = readableStreamFromArray([chunk]);
    const output = input.pipeThrough(new TextDecoderStream());
    const array = await readableStreamToArray(output);
    assert_array_equals(array, ['\uFFFD']);
  }, 'UTF-8 EOF handling - ' + arrayBufferOrSharedArrayBuffer);
});

promise_test(async () => {
  const buffer = new ArrayBuffer(3);
  const view = new Uint8Array(buffer, 1, 1);
  view[0] = 65;
  new MessageChannel().port1.postMessage(buffer, [buffer]);
  const input = readableStreamFromArray([view]);
  const output = input.pipeThrough(new TextDecoderStream());
  const array = await readableStreamToArray(output);
  assert_array_equals(array, [], 'no chunks should be output');
}, 'decoding a transferred Uint8Array chunk should give no output');

promise_test(async () => {
  const buffer = new ArrayBuffer(1);
  new MessageChannel().port1.postMessage(buffer, [buffer]);
  const input = readableStreamFromArray([buffer]);
  const output = input.pipeThrough(new TextDecoderStream());
  const array = await readableStreamToArray(output);
  assert_array_equals(array, [], 'no chunks should be output');
}, 'decoding a transferred ArrayBuffer chunk should give no output');
