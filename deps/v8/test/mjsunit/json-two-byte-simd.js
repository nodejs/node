// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

for (const n of [7, 8, 9, 15, 16, 17]) {
  const prefix = 'a'.repeat(n) + '\u0100'.repeat(32);
  for (const [value, encoded] of [
    ['"', '\\"'], ['\\', '\\\\'], ['\n', '\\n'],
    ['\ud800', '\\ud800'], ['\udfff', '\\udfff'], ['\ud83d\ude00', '\ud83d\ude00'],
  ]) {
    const text = prefix + value + '\u0101'.repeat(5);
    const json = '"' + prefix + encoded + '\u0101'.repeat(5) + '"';
    assertEquals(json, JSON.stringify(text));
    assertEquals(text, JSON.parse(json));
  }
  assertThrows(() => JSON.parse('"' + prefix + '\n"'), SyntaxError);
}
