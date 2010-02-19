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

// Flags: --allow-natives-syntax

function RunTest(ensure_fast_case) {
  function C1() {
    this.x = 23;
  };
  C1.prototype = { set x(value) { this.y = 23; } };
  if (ensure_fast_case) {
    %ToFastProperties(C1.prototype);
  }
  
  for (var i = 0; i < 10; i++) {
    var c1 = new C1();
    assertEquals("undefined", typeof c1.x);
    assertEquals(23, c1.y);
  }
  
  
  function C2() {
    this.x = 23;
  };
  C2.prototype = { };
  C2.prototype.__proto__ = { set x(value) { this.y = 23; } };
  if (ensure_fast_case) {
    %ToFastProperties(C2.prototype.__proto__)
  }
  
  for (var i = 0; i < 10; i++) {
    var c2 = new C2();
    assertEquals("undefined", typeof c2.x);
    assertEquals(23, c2.y);
  }
  
  
  function C3() {
    this.x = 23;
  };
  C3.prototype = { };
  C3.prototype.__defineSetter__('x', function(value) { this.y = 23; });
  if (ensure_fast_case) {
    %ToFastProperties(C3.prototype);
  }
  
  for (var i = 0; i < 10; i++) {
    var c3 = new C3();
    assertEquals("undefined", typeof c3.x);
    assertEquals(23, c3.y);
  }
  
  
  function C4() {
    this.x = 23;
  };
  C4.prototype = { };
  C4.prototype.__proto__ = {  };
  C4.prototype.__proto__.__defineSetter__('x', function(value) { this.y = 23; });
  if (ensure_fast_case) {
    %ToFastProperties(C4.prototype.__proto__);
  }
  
  for (var i = 0; i < 10; i++) {
    var c4 = new C4();
    assertEquals("undefined", typeof c4.x);
    assertEquals(23, c4.y);
  }
  
  
  function D() {
    this.x = 23;
  };
  D.prototype = 1;
  if (ensure_fast_case) {
    %ToFastProperties(D.prototype);
  }
  
  for (var i = 0; i < 10; i++) {
    var d = new D();
    assertEquals(23, d.x);
    assertEquals("undefined", typeof d.y);
  }
}

RunTest(false);
RunTest(true);
