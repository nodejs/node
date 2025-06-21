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

function Hash() {
  for (var i = 0; i < 100; i++) {
    this['a' + i] = i;
  }

  delete this.a50;  // Ensure it's a normal object.
}

Hash.prototype.m = function() {
  return 1;
};

var h = new Hash();

for (var i = 1; i < 100; i++) {
  if (i == 50) {
    h.m = function() {
      return 2;
    };
  } else if (i == 70) {
    delete h.m;
  }
  assertEquals(i < 50 || i >= 70 ? 1 : 2, h.m());
}


var nonsymbol = 'wwwww '.split(' ')[0];
Hash.prototype.wwwww = Hash.prototype.m;

for (var i = 1; i < 100; i++) {
  if (i == 50) {
    h[nonsymbol] = function() {
      return 2;
    };
  } else if (i == 70) {
    delete h[nonsymbol];
  }
  assertEquals(i < 50 || i >= 70 ? 1 : 2, h.wwwww());
}
