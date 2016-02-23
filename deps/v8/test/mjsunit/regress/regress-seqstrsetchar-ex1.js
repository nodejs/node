// Copyright 2013 the V8 project authors. All rights reserved.
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

// Flags: --allow-natives-syntax

// stubbed version of ToNumber
function ToNumber(x) {
  return 311;
}

// Reduced version of String.fromCharCode;
// does not actually do the same calculation but exhibits untagging bug.
function StringFromCharCode(code) {
  var n = %_ArgumentsLength();
  var one_byte = %NewString(n, true);
  var i;
  for (i = 0; i < n; i++) {
    var code = %_Arguments(i);
    if (!%_IsSmi(code)) code = ToNumber(code) & 0xffff;
    if (code > 0xff) break;
  }

  var two_byte = %NewString(n - i, false);
  for (var j = 0; i < n; i++, j++) {
    var code = %_Arguments(i);
    %_TwoByteSeqStringSetChar(j, code, two_byte);
  }
  return one_byte + two_byte;
}

StringFromCharCode(0xFFF, 0xFFF);
StringFromCharCode(0x7C, 0x7C);
%OptimizeFunctionOnNextCall(StringFromCharCode);
StringFromCharCode(0x7C, 0x7C);
StringFromCharCode(0xFFF, 0xFFF);
