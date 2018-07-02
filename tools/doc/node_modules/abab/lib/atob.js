'use strict';

/**
 * Implementation of atob() according to the HTML spec, except that instead of
 * throwing INVALID_CHARACTER_ERR we return null.
 */
function atob(input) {
  // WebIDL requires DOMStrings to just be converted using ECMAScript
  // ToString, which in our case amounts to calling String().
  input = String(input);
  // "Remove all space characters from input."
  input = input.replace(/[ \t\n\f\r]/g, '');
  // "If the length of input divides by 4 leaving no remainder, then: if
  // input ends with one or two U+003D EQUALS SIGN (=) characters, remove
  // them from input."
  if (input.length % 4 == 0 && /==?$/.test(input)) {
    input = input.replace(/==?$/, '');
  }
  // "If the length of input divides by 4 leaving a remainder of 1, throw an
  // INVALID_CHARACTER_ERR exception and abort these steps."
  //
  // "If input contains a character that is not in the following list of
  // characters and character ranges, throw an INVALID_CHARACTER_ERR
  // exception and abort these steps:
  //
  // U+002B PLUS SIGN (+)
  // U+002F SOLIDUS (/)
  // U+0030 DIGIT ZERO (0) to U+0039 DIGIT NINE (9)
  // U+0041 LATIN CAPITAL LETTER A to U+005A LATIN CAPITAL LETTER Z
  // U+0061 LATIN SMALL LETTER A to U+007A LATIN SMALL LETTER Z"
  if (input.length % 4 == 1 || !/^[+/0-9A-Za-z]*$/.test(input)) {
    return null;
  }
  // "Let output be a string, initially empty."
  var output = '';
  // "Let buffer be a buffer that can have bits appended to it, initially
  // empty."
  //
  // We append bits via left-shift and or.  accumulatedBits is used to track
  // when we've gotten to 24 bits.
  var buffer = 0;
  var accumulatedBits = 0;
  // "While position does not point past the end of input, run these
  // substeps:"
  for (var i = 0; i < input.length; i++) {
    // "Find the character pointed to by position in the first column of
    // the following table. Let n be the number given in the second cell of
    // the same row."
    //
    // "Append to buffer the six bits corresponding to number, most
    // significant bit first."
    //
    // atobLookup() implements the table from the spec.
    buffer <<= 6;
    buffer |= atobLookup(input[i]);
    // "If buffer has accumulated 24 bits, interpret them as three 8-bit
    // big-endian numbers. Append the three characters with code points
    // equal to those numbers to output, in the same order, and then empty
    // buffer."
    accumulatedBits += 6;
    if (accumulatedBits == 24) {
      output += String.fromCharCode((buffer & 0xff0000) >> 16);
      output += String.fromCharCode((buffer & 0xff00) >> 8);
      output += String.fromCharCode(buffer & 0xff);
      buffer = accumulatedBits = 0;
    }
    // "Advance position by one character."
  }
  // "If buffer is not empty, it contains either 12 or 18 bits. If it
  // contains 12 bits, discard the last four and interpret the remaining
  // eight as an 8-bit big-endian number. If it contains 18 bits, discard the
  // last two and interpret the remaining 16 as two 8-bit big-endian numbers.
  // Append the one or two characters with code points equal to those one or
  // two numbers to output, in the same order."
  if (accumulatedBits == 12) {
    buffer >>= 4;
    output += String.fromCharCode(buffer);
  } else if (accumulatedBits == 18) {
    buffer >>= 2;
    output += String.fromCharCode((buffer & 0xff00) >> 8);
    output += String.fromCharCode(buffer & 0xff);
  }
  // "Return output."
  return output;
}
/**
 * A lookup table for atob(), which converts an ASCII character to the
 * corresponding six-bit number.
 */
function atobLookup(chr) {
  if (/[A-Z]/.test(chr)) {
    return chr.charCodeAt(0) - 'A'.charCodeAt(0);
  }
  if (/[a-z]/.test(chr)) {
    return chr.charCodeAt(0) - 'a'.charCodeAt(0) + 26;
  }
  if (/[0-9]/.test(chr)) {
    return chr.charCodeAt(0) - '0'.charCodeAt(0) + 52;
  }
  if (chr == '+') {
    return 62;
  }
  if (chr == '/') {
    return 63;
  }
  // Throw exception; should not be hit in tests
}

module.exports = atob;
