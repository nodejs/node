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

// Test that exceptions are not thrown when setting properties on object
// that have only a getter in a prototype object, except when we are in strict
// mode where exceptsions should be thrown.

var o = {};
var p = {};
p.__defineGetter__('x', function(){});
p.__defineGetter__(0, function(){});
o.__proto__ = p;

assertDoesNotThrow("o.x = 42");
assertDoesNotThrow("o[0] = 42");

assertThrows(function() { 'use strict'; o.x = 42; });
assertThrows(function() { 'use strict'; o[0] = 42; });

function f() {
  with(o) {
    x = 42;
  }
}

assertDoesNotThrow(f);

__proto__ = p;
function g() {
  eval('1');
  x = 42;
}

function g_strict() {
  'use strict';
  eval('1');
  x = 42;
}

assertDoesNotThrow(g);
assertThrows(g_strict);

__proto__ = p;
function g2() {
  this[0] = 42;
}

function g2_strict() {
  'use strict';
  this[0] = 42;
}

assertDoesNotThrow(g2);
assertThrows(g2_strict);
