// Copyright 2012 the V8 project authors. All rights reserved.
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

function RunOneTruncationTest(a, b) {
  var temp = a | 0;
  assertEquals(b, temp);
};
%PrepareFunctionForOptimization(RunOneTruncationTest);
function RunAllTruncationTests() {
  RunOneTruncationTest(0, 0);
  RunOneTruncationTest(0.5, 0);
  RunOneTruncationTest(-0.5, 0);
  RunOneTruncationTest(1.5, 1);
  RunOneTruncationTest(-1.5, -1);
  RunOneTruncationTest(5.5, 5);
  RunOneTruncationTest(-5.0, -5);
  RunOneTruncationTest(NaN, 0);
  RunOneTruncationTest(Infinity, 0);
  RunOneTruncationTest(-NaN, 0);
  RunOneTruncationTest(-Infinity, 0);

  RunOneTruncationTest(4.5036e+15, 0x1635E000);
  RunOneTruncationTest(-4.5036e+15, -372629504);

  RunOneTruncationTest(4503603922337791.0, -1);
  RunOneTruncationTest(-4503603922337791.0, 1);
  RunOneTruncationTest(4503601774854143.0, 2147483647);
  RunOneTruncationTest(-4503601774854143.0, -2147483647);
  RunOneTruncationTest(9007207844675582.0, -2);
  RunOneTruncationTest(-9007207844675582.0, 2);

  RunOneTruncationTest(2.4178527921507624e+24, -536870912);
  RunOneTruncationTest(-2.4178527921507624e+24, 536870912);
  RunOneTruncationTest(2.417853945072267e+24, -536870912);
  RunOneTruncationTest(-2.417853945072267e+24, 536870912);

  RunOneTruncationTest(4.8357055843015248e+24, -1073741824);
  RunOneTruncationTest(-4.8357055843015248e+24, 1073741824);
  RunOneTruncationTest(4.8357078901445341e+24, -1073741824);
  RunOneTruncationTest(-4.8357078901445341e+24, 1073741824);

  RunOneTruncationTest(9.6714111686030497e+24, -2147483648);
  RunOneTruncationTest(-9.6714111686030497e+24, -2147483648);
  RunOneTruncationTest(9.6714157802890681e+24, -2147483648);
  RunOneTruncationTest(-9.6714157802890681e+24, -2147483648);
}

RunAllTruncationTests();
RunAllTruncationTests();
%OptimizeFunctionOnNextCall(RunOneTruncationTest);
RunAllTruncationTests();
RunAllTruncationTests();
