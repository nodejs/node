// Copyright 2015 the V8 project authors. All rights reserved.
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

// Flags: --max-old-space-size=60 --check-handle-count --no-concurrent-sparkplug

table = [];

for (var i = 0; i < 32; i++) {
 table[i] = String.fromCharCode(i + 0x410);
}


var random = (function() {
  var seed = 10;
  return function() {
    seed = (seed * 1009) % 8831;
    return seed;
  };
})();


function key(length) {
  var s = "";
  for (var i = 0; i < length; i++) {
    s += table[random() % 32];
  }
  return '"' + s + '"';
}


function value() {
  return '[{' + '"field1" : ' + random() + ', "field2" : ' + random() + '}]';
}


function generate(n) {
  var s = '{';
  for (var i = 0; i < n; i++) {
     if (i > 0) s += ', ';
     s += key(random() % 10 + 7);
     s += ':';
     s += value();
  }
  s += '}';
  return s;
}


print("generating");

var str = generate(10000);

print("parsing "  + str.length);
JSON.parse(str);

print("done");
