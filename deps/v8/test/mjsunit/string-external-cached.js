// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Flags: --expose-externalize-string --expose-gc
// Test data pointer caching of external strings.

function test() {
  // Test string.charAt.
  var charat_str = new Array(5);
  charat_str[0] = createExternalizableString('0123456789ABCDEF0123456789ABCDEF\
0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF\
0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF\
0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF\
0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF');
  charat_str[1] = createExternalizableString('0123456789ABCDEF');
  for (var i = 0; i < 6; i++) charat_str[1] += charat_str[1];
  try {  // String can only be externalized once
    externalizeString(charat_str[0]);
    externalizeString(charat_str[1]);
  } catch (ex) { }
  charat_str[2] = charat_str[0].slice(0, -1);
  charat_str[3] = charat_str[1].slice(0, -1);
  charat_str[4] = charat_str[0] + charat_str[0];

  for (var i = 0; i < 5; i++) {
    assertEquals('B', charat_str[i].charAt(6*16 + 11));
    assertEquals('C', charat_str[i].charAt(6*16 + 12));
    assertEquals('A', charat_str[i].charAt(3*16 + 10));
    assertEquals('B', charat_str[i].charAt(3*16 + 11));
  }

  charat_short = createExternalizableString('01234');
  try {  // String can only be externalized once
    externalizeString(charat_short);
  } catch (ex) { }
  assertEquals("1", charat_short.charAt(1));

  // Test regexp and short substring.
  var re = /(A|B)/;
  var rere = /(T.{1,3}B)/u;
  var ascii = createExternalizableString('ABCDEFGHIJKLMNOPQRST');
  var twobyte = createExternalizableString('_ABCDEFGHIJKLMNOPQRST🤓');
  try {  // String can only be externalized once
    externalizeString(ascii);
    externalizeString(twobyte);
  } catch (ex) { }
  assertTrue(isOneByteString(ascii));
  assertFalse(isOneByteString(twobyte));
  var ascii_slice = ascii.slice(1,-1);
  var twobyte_slice = twobyte.slice(2,-1);
  var ascii_cons = ascii + ascii;
  var twobyte_cons = twobyte + twobyte;
  for (var i = 0; i < 2; i++) {
    assertEquals(["A", "A"], re.exec(ascii));
    assertEquals(["B", "B"], re.exec(ascii_slice));
    assertEquals(["TAB", "TAB"], rere.exec(ascii_cons));
    assertEquals(["A", "A"], re.exec(twobyte));
    assertEquals(["B", "B"], re.exec(twobyte_slice));
    assertEquals(["T🤓_AB", "T🤓_AB"], rere.exec(twobyte_cons));
    assertEquals("DEFG", ascii_slice.substr(2, 4));
    assertEquals("DEFG", twobyte_slice.substr(2, 4));
    assertEquals("DEFG", ascii_cons.substr(3, 4));
    assertEquals("DEFG", twobyte_cons.substr(4, 4));
  }

  // Test adding external strings
  var long_ascii = createExternalizableString('MCsquared');
  var long_twobyte = createExternalizableString('MCsquare\u1234');
  var min_short_ascii = 'E='.padEnd(kExternalStringMinOneByteLength, '.');
  var min_short_twobyte =
      'E=\u1234'.padEnd(kExternalStringMinTwoByteLength, '\u1234');
  var short_ascii = createExternalizableString(min_short_ascii);
  var short_twobyte = createExternalizableString(min_short_twobyte);
  try {  // String can only be externalized once
    externalizeString(short_ascii);
    externalizeString(long_ascii);
    externalizeString(short_twobyte);
    externalizeString(long_twobyte);
    assertTrue(isOneByteString(short_asii) && isOneByteString(long_ascii));
    assertFalse(isOneByteString(short_twobyte) || isOneByteString(long_twobyte));
  } catch (ex) { }
  assertEquals(min_short_ascii + 'MCsquared', short_ascii + long_ascii);
  assertTrue(isOneByteString(short_ascii + long_ascii));
  assertEquals('MCsquared' + min_short_ascii, long_ascii + short_ascii);
  assertEquals(
      min_short_twobyte + 'MCsquare\u1234', short_twobyte + long_twobyte);
  assertFalse(isOneByteString(short_twobyte + long_twobyte));
  assertEquals("E=MCsquared", "E=" + long_ascii);
  assertEquals(min_short_twobyte + 'MCsquared', short_twobyte + 'MCsquared');
  assertEquals(min_short_twobyte + 'MCsquared', short_twobyte + long_ascii);
  assertFalse(isOneByteString(short_twobyte + long_ascii));
}

// Run the test many times to ensure IC-s don't break things.
for (var i = 0; i < 10; i++) {
  test();
}

// Clean up string to make Valgrind happy.
gc();
gc();
