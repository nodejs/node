// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for JSON string parsing and stringification around the SIMD block
// boundary (16 bytes). These exercise the handoff between SIMD fast path and
// scalar fallback.

// --- JSON.parse: plain strings of varying lengths around 16-byte boundary ---
for (let len = 0; len <= 33; len++) {
  let s = 'a'.repeat(len);
  assertEquals(s, JSON.parse('"' + s + '"'));
}

// --- JSON.parse: special char at every position in a 32-byte string ---
// Test '"' (escaped quote does not terminate the string).
for (let pos = 0; pos <= 31; pos++) {
  let prefix = 'a'.repeat(pos);
  let suffix = 'b'.repeat(31 - pos);
  let json = '"' + prefix + '\\"' + suffix + '"';
  assertEquals(prefix + '"' + suffix, JSON.parse(json));
}

// Test '\\' (backslash triggers escape handling).
for (let pos = 0; pos <= 31; pos++) {
  let prefix = 'a'.repeat(pos);
  let suffix = 'b'.repeat(31 - pos);
  let json = '"' + prefix + '\\n' + suffix + '"';
  assertEquals(prefix + '\n' + suffix, JSON.parse(json));
}

// Test control character (< 0x20) causes parse error.
for (let pos = 0; pos <= 31; pos++) {
  let prefix = 'a'.repeat(pos);
  let suffix = 'b'.repeat(31 - pos);
  // Embed a raw \x01 control character which is illegal in JSON strings.
  let json = '"' + prefix + '\x01' + suffix + '"';
  assertThrows(() => JSON.parse(json), SyntaxError);
}

// --- JSON.parse: strings exactly at SIMD boundaries ---
// 15 bytes (no full SIMD block), 16 bytes (exactly one block), 17 bytes (one
// block + 1 tail byte).
for (let len of [15, 16, 17, 31, 32, 33]) {
  let s = 'x'.repeat(len);
  assertEquals(s, JSON.parse('"' + s + '"'));

  // With escape at the end.
  let withEscape = 'x'.repeat(len - 1) + '\\n';
  assertEquals('x'.repeat(len - 1) + '\n', JSON.parse('"' + withEscape + '"'));

  // With escape at the start.
  withEscape = '\\n' + 'x'.repeat(len - 1);
  assertEquals('\n' + 'x'.repeat(len - 1), JSON.parse('"' + withEscape + '"'));
}

// --- JSON.stringify: strings of varying lengths around 16-byte boundary ---
for (let len = 0; len <= 33; len++) {
  let s = 'a'.repeat(len);
  assertEquals('"' + s + '"', JSON.stringify(s));
}

// --- JSON.stringify: special char at every position in a 32-byte string ---
// Backslash.
for (let pos = 0; pos <= 31; pos++) {
  let prefix = 'a'.repeat(pos);
  let suffix = 'b'.repeat(31 - pos);
  let input = prefix + '\\' + suffix;
  let expected = '"' + prefix + '\\\\' + suffix + '"';
  assertEquals(expected, JSON.stringify(input));
}

// Quote.
for (let pos = 0; pos <= 31; pos++) {
  let prefix = 'a'.repeat(pos);
  let suffix = 'b'.repeat(31 - pos);
  let input = prefix + '"' + suffix;
  let expected = '"' + prefix + '\\"' + suffix + '"';
  assertEquals(expected, JSON.stringify(input));
}

// Control character (newline).
for (let pos = 0; pos <= 31; pos++) {
  let prefix = 'a'.repeat(pos);
  let suffix = 'b'.repeat(31 - pos);
  let input = prefix + '\n' + suffix;
  let expected = '"' + prefix + '\\n' + suffix + '"';
  assertEquals(expected, JSON.stringify(input));
}

// Control character (\x00).
for (let pos = 0; pos <= 31; pos++) {
  let prefix = 'a'.repeat(pos);
  let suffix = 'b'.repeat(31 - pos);
  let input = prefix + '\x00' + suffix;
  let expected = '"' + prefix + '\\u0000' + suffix + '"';
  assertEquals(expected, JSON.stringify(input));
}

// --- JSON roundtrip: parse(stringify(x)) === x ---
for (let len = 0; len <= 33; len++) {
  let s = 'z'.repeat(len);
  assertEquals(s, JSON.parse(JSON.stringify(s)));
}

// Roundtrip with escapes at various positions.
for (let pos = 0; pos <= 17; pos++) {
  let s = 'a'.repeat(pos) + '\n' + 'b'.repeat(17 - pos);
  assertEquals(s, JSON.parse(JSON.stringify(s)));
  s = 'a'.repeat(pos) + '\\' + 'b'.repeat(17 - pos);
  assertEquals(s, JSON.parse(JSON.stringify(s)));
  s = 'a'.repeat(pos) + '"' + 'b'.repeat(17 - pos);
  assertEquals(s, JSON.parse(JSON.stringify(s)));
}

// --- Multiple special characters in the same SIMD block ---
for (let len of [16, 32]) {
  // All escapes.
  let allEscapes = '\\n'.repeat(len / 2);
  let expected = '\n'.repeat(len / 2);
  assertEquals(expected, JSON.parse('"' + allEscapes + '"'));

  // Alternating normal and escape chars.
  let mixed = '';
  let mixedExpected = '';
  for (let i = 0; i < len; i++) {
    if (i % 3 === 0) {
      mixed += '\\t';
      mixedExpected += '\t';
    } else {
      mixed += 'a';
      mixedExpected += 'a';
    }
  }
  assertEquals(mixedExpected, JSON.parse('"' + mixed + '"'));
}
