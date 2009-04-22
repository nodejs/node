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

const SMI_MAX = (1 << 30) - 1;
const SMI_MIN = -(1 << 30);
const ONE = 1;
const ONE_HUNDRED = 100;

const OBJ_42 = new (function() {
  this.valueOf = function() { return 42; };
})();

assertEquals(42, OBJ_42.valueOf()); 


function Add1(x) {
  return x + 1;
}

function Add100(x) {
  return x + 100;
}

function Add1Reversed(x) {
  return 1 + x;
}

function Add100Reversed(x) {
  return 100 + x;
}


assertEquals(1, Add1(0));  // fast case
assertEquals(1, Add1Reversed(0));  // fast case
assertEquals(SMI_MAX + ONE, Add1(SMI_MAX));  // overflow
assertEquals(SMI_MAX + ONE, Add1Reversed(SMI_MAX));  // overflow
assertEquals(42 + ONE, Add1(OBJ_42));  // non-smi
assertEquals(42 + ONE, Add1Reversed(OBJ_42));  // non-smi

assertEquals(100, Add100(0));  // fast case
assertEquals(100, Add100Reversed(0));  // fast case
assertEquals(SMI_MAX + ONE_HUNDRED, Add100(SMI_MAX));  // overflow
assertEquals(SMI_MAX + ONE_HUNDRED, Add100Reversed(SMI_MAX));  // overflow
assertEquals(42 + ONE_HUNDRED, Add100(OBJ_42));  // non-smi
assertEquals(42 + ONE_HUNDRED, Add100Reversed(OBJ_42));  // non-smi



function Sub1(x) {
  return x - 1;
}

function Sub100(x) {
  return x - 100;
}

function Sub1Reversed(x) {
  return 1 - x;
}

function Sub100Reversed(x) {
  return 100 - x;
}


assertEquals(0, Sub1(1));  // fast case
assertEquals(-1, Sub1Reversed(2));  // fast case
assertEquals(SMI_MIN - ONE, Sub1(SMI_MIN));  // overflow
assertEquals(ONE - SMI_MIN, Sub1Reversed(SMI_MIN));  // overflow
assertEquals(42 - ONE, Sub1(OBJ_42));  // non-smi
assertEquals(ONE - 42, Sub1Reversed(OBJ_42));  // non-smi

assertEquals(0, Sub100(100));  // fast case
assertEquals(1, Sub100Reversed(99));  // fast case
assertEquals(SMI_MIN - ONE_HUNDRED, Sub100(SMI_MIN));  // overflow
assertEquals(ONE_HUNDRED - SMI_MIN, Sub100Reversed(SMI_MIN));  // overflow
assertEquals(42 - ONE_HUNDRED, Sub100(OBJ_42));  // non-smi
assertEquals(ONE_HUNDRED - 42, Sub100Reversed(OBJ_42));  // non-smi
