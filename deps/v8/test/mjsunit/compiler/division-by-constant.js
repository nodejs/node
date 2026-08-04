// Copyright 2014 the V8 project authors. All rights reserved.
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

// -----------------------------------------------------------------------------

function ConstructDiv(divisor) {
  return "return ((dividend | 0) / ((" + divisor  + ") | 0)) | 0";
}

var RefDivByConstI =
  new Function("dividend", "divisor", ConstructDiv("divisor"));

%NeverOptimizeFunction(RefDivByConstI);

// -----------------------------------------------------------------------------

function ConstructMod(divisor) {
  return "return ((dividend | 0) % ((" + divisor  + ") | 0)) | 0";
}

var RefModByConstI =
  new Function("dividend", "divisor", ConstructMod("divisor"));

%NeverOptimizeFunction(RefModByConstI);

// -----------------------------------------------------------------------------

function ConstructFlooringDiv(divisor) {
  return "return Math.floor(dividend / (" + divisor  + ")) | 0";
}

var RefFlooringDivByConstI =
  new Function("dividend", "divisor", ConstructFlooringDiv("divisor"));

%NeverOptimizeFunction(RefFlooringDivByConstI);

// -----------------------------------------------------------------------------

function PushSymmetric(values, x) {
    values.push(x, -x);
}

function PushRangeSymmetric(values, from, to) {
  for (var x = from; x <= to; x++) {
    PushSymmetric(values, x);
  }
}

function CreateTestValues() {
  var values = [
    // -(2^31)
    -2147483648,
    // Some values from "Hacker's Delight", chapter 10-7.
    715827883, 1431655766, -1431655765, -1431655764,
    // Some "randomly" chosen numbers.
    123, -1234, 12345, -123456, 1234567, -12345678, 123456789
  ];
  // Powers of 2
  for (var shift = 6; shift < 31; shift++) {
    PushSymmetric(values, 1 << shift);
  }
  // Values near zero
  PushRangeSymmetric(values, 1, 32);
  // Various magnitudes
  PushRangeSymmetric(values, 100, 109);
  PushRangeSymmetric(values, 1000, 1009);
  PushRangeSymmetric(values, 10000, 10009);
  PushRangeSymmetric(values, 100000, 100009);
  PushRangeSymmetric(values, 1000000, 1000009);
  PushRangeSymmetric(values, 10000000, 10000009);
  PushRangeSymmetric(values, 100000000, 100000009);
  PushRangeSymmetric(values, 1000000000, 1000000009);
  return values;
}

// -----------------------------------------------------------------------------


function TestDivisionLike(ref, construct, values, divisor) {
  // Define the function to test.
  var OptFun = new Function("dividend", construct(divisor));

  // Warm up type feedback.
  %PrepareFunctionForOptimization(OptFun);
  OptFun(7);
  OptFun(11);
  %OptimizeFunctionOnNextCall(OptFun);
  OptFun(13);

function dude(dividend) {
    // Avoid deopt caused by overflow, we do not want to test this here.
    if (dividend === -2147483648 && divisor === -1) return;
    assertEquals(ref(dividend, divisor), OptFun(dividend));
  }

  // Check results.
  values.forEach(dude);
}

function Test(ref, construct) {
  var values = CreateTestValues();
  values.forEach(function(divisor) {
    TestDivisionLike(ref, construct, values, divisor);
  });
}

Test(RefDivByConstI, ConstructDiv);
Test(RefModByConstI, ConstructMod);
Test(RefFlooringDivByConstI, ConstructFlooringDiv);
