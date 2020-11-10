// META: global=worker
// META: script=resources/readable-stream-from-array.js
// META: script=resources/readable-stream-to-array.js

'use strict';
const inputString = 'I \u{1F499} streams';
const expectedOutputBytes = [0x49, 0x20, 0xf0, 0x9f, 0x92, 0x99, 0x20, 0x73,
                             0x74, 0x72, 0x65, 0x61, 0x6d, 0x73];
// This is a character that must be represented in two code units in a string,
// ie. it is not in the Basic Multilingual Plane.
const astralCharacter = '\u{1F499}';  // BLUE HEART
const astralCharacterEncoded = [0xf0, 0x9f, 0x92, 0x99];
const leading = astralCharacter[0];
const trailing = astralCharacter[1];
const replacementEncoded = [0xef, 0xbf, 0xbd];

// These tests assume that the implementation correctly classifies leading and
// trailing surrogates and treats all the code units in each set equivalently.

const testCases = [
  {
    input: [inputString],
    output: [expectedOutputBytes],
    description: 'encoding one string of UTF-8 should give one complete chunk'
  },
  {
    input: [leading, trailing],
    output: [astralCharacterEncoded],
    description: 'a character split between chunks should be correctly encoded'
  },
  {
    input: [leading, trailing + astralCharacter],
    output: [astralCharacterEncoded.concat(astralCharacterEncoded)],
    description: 'a character following one split between chunks should be ' +
        'correctly encoded'
  },
  {
    input: [leading, trailing + leading, trailing],
    output: [astralCharacterEncoded, astralCharacterEncoded],
    description: 'two consecutive astral characters each split down the ' +
        'middle should be correctly reassembled'
  },
  {
    input: [leading, trailing + leading + leading, trailing],
    output: [astralCharacterEncoded.concat(replacementEncoded), astralCharacterEncoded],
    description: 'two consecutive astral characters each split down the ' +
        'middle with an invalid surrogate in the middle should be correctly ' +
        'encoded'
  },
  {
    input: [leading],
    output: [replacementEncoded],
    description: 'a stream ending in a leading surrogate should emit a ' +
        'replacement character as a final chunk'
  },
  {
    input: [leading, astralCharacter],
    output: [replacementEncoded.concat(astralCharacterEncoded)],
    description: 'an unmatched surrogate at the end of a chunk followed by ' +
        'an astral character in the next chunk should be replaced with ' +
        'the replacement character at the start of the next output chunk'
  },
  {
    input: [leading, 'A'],
    output: [replacementEncoded.concat([65])],
    description: 'an unmatched surrogate at the end of a chunk followed by ' +
        'an ascii character in the next chunk should be replaced with ' +
        'the replacement character at the start of the next output chunk'
  },
  {
    input: [leading, leading, trailing],
    output: [replacementEncoded, astralCharacterEncoded],
    description: 'an unmatched surrogate at the end of a chunk followed by ' +
        'a plane 1 character split into two chunks should result in ' +
        'the encoded plane 1 character appearing in the last output chunk'
  },
  {
    input: [leading, leading],
    output: [replacementEncoded, replacementEncoded],
    description: 'two leading chunks should result in two replacement ' +
        'characters'
  },
  {
    input: [leading + leading, trailing],
    output: [replacementEncoded, astralCharacterEncoded],
    description: 'a non-terminal unpaired leading surrogate should ' +
        'immediately be replaced'
  },
  {
    input: [trailing, astralCharacter],
    output: [replacementEncoded, astralCharacterEncoded],
    description: 'a terminal unpaired trailing surrogate should ' +
        'immediately be replaced'
  },
  {
    input: [leading, '', trailing],
    output: [astralCharacterEncoded],
    description: 'a leading surrogate chunk should be carried past empty chunks'
  },
  {
    input: [leading, ''],
    output: [replacementEncoded],
    description: 'a leading surrogate chunk should error when it is clear ' +
        'it didn\'t form a pair'
  },
  {
    input: [''],
    output: [],
    description: 'an empty string should result in no output chunk'
  },
  {
    input: ['', inputString],
    output: [expectedOutputBytes],
    description: 'a leading empty chunk should be ignored'
  },
  {
    input: [inputString, ''],
    output: [expectedOutputBytes],
    description: 'a trailing empty chunk should be ignored'
  },
  {
    input: ['A'],
    output: [[65]],
    description: 'a plain ASCII chunk should be converted'
  },
  {
    input: ['\xff'],
    output: [[195, 191]],
    description: 'characters in the ISO-8859-1 range should be encoded correctly'
  },
];

for (const {input, output, description} of testCases) {
  promise_test(async () => {
    const inputStream = readableStreamFromArray(input);
    const outputStream = inputStream.pipeThrough(new TextEncoderStream());
    const chunkArray = await readableStreamToArray(outputStream);
    assert_equals(chunkArray.length, output.length,
                  'number of chunks should match');
    for (let i = 0; i < output.length; ++i) {
      assert_array_equals(chunkArray[i], output[i], `chunk ${i} should match`);
    }
  }, description);
}
