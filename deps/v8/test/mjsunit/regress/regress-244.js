// Copyright 2009 the V8 project authors. All rights reserved.
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

var kLegalPairs = [
  [0x00, '%00'],
  [0x01, '%01'],
  [0x7f, '%7F'],
  [0x80, '%C2%80'],
  [0x81, '%C2%81'],
  [0x7ff, '%DF%BF'],
  [0x800, '%E0%A0%80'],
  [0x801, '%E0%A0%81'],
  [0xd7ff, '%ED%9F%BF'],
  [0xffff, '%EF%BF%BF']
];

var kIllegalEncoded = [
  '%80', '%BF', '%80%BF', '%80%BF%80', '%C0%22', '%DF',
  '%EF%BF', '%F7BFBF', '%FE', '%FF', '%FE%FE%FF%FF',
  '%C0%AF', '%E0%9F%BF', '%F0%8F%BF%BF', '%C0%80',
  '%E0%80%80'
];

function run() {
  for (var i = 0; i < kLegalPairs.length; i++) {
    var decoded = String.fromCharCode(kLegalPairs[i][0]);
    var encoded = kLegalPairs[i][1];
    assertEquals(decodeURI(encoded), decoded);
    assertEquals(encodeURI(decoded), encoded);
  }
  for (var i = 0; i < kIllegalEncoded.length; i++) {
    var value = kIllegalEncoded[i];
    var exception = false;
    try {
      decodeURI(value);
    } catch (e) {
      exception = true;
      assertInstanceof(e, URIError);
    }
    assertTrue(exception);
  }
}

run();
