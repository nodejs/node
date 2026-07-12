// Copyright 2008 the V8 project authors. All rights reserved.
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

// Ensure that we can correctly change the sign of the most negative smi.

// Possible Smi ranges.
var ranges = [{min: -1073741824, max: 1073741823, bits: 31},
              {min: -2147483648, max: 2147483647, bits: 32}];

for (var i = 0; i < ranges.length; i++) {
  var range = ranges[i];
  var min_smi = range.min;
  var max_smi = range.max;
  var bits = range.bits;
  var name = bits + "-bit";

  var result = max_smi + 1;

  // Min smi as literal
  assertEquals(result, eval(min_smi + " * -1"), name + "-litconmult");
  assertEquals(result, eval(min_smi + " / -1"), name + "-litcondiv");
  assertEquals(result, eval("-(" + min_smi + ")"), name + "-litneg");
  assertEquals(result, eval("0 - (" + min_smi + ")")), name + "-conlitsub";

  // As variable:
  assertEquals(result, min_smi * -1, name + "-varconmult");
  assertEquals(result, min_smi / -1, name + "-varcondiv");
  assertEquals(result, -min_smi, name + "-varneg");
  assertEquals(result, 0 - min_smi, name + "-convarsub");

  // Only variables:
  var zero = 0;
  var minus_one = -1;

  assertEquals(result, min_smi * minus_one, name + "-varvarmult");
  assertEquals(result, min_smi / minus_one, name + "-varvardiv");
  assertEquals(result, zero - min_smi, name + "-varvarsub");

  // Constants as variables
  assertEquals(result, eval(min_smi + " * minus_one"), name + "-litvarmult");
  assertEquals(result, eval(min_smi + " / minus_one"), name + "-litvarmdiv");
  assertEquals(result, eval("0 - (" + min_smi + ")"), name + "-varlitsub");

  var half_min_smi = -(1 << (bits >> 1));
  var half_max_smi = 1 << ((bits - 1) >> 1);

  assertEquals(max_smi + 1, -half_min_smi * half_max_smi, name + "-half1");
  assertEquals(max_smi + 1, half_min_smi * -half_max_smi, name + "-half2");
  assertEquals(max_smi + 1, half_max_smi * -half_min_smi, name + "-half3");
  assertEquals(max_smi + 1, -half_max_smi * half_min_smi, name + "-half4");
}
