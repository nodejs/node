// Copyright 2011 the V8 project authors. All rights reserved.
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

// Flags: --harmony-scoping

// Test for conflicting variable bindings.

// TODO(ES6): properly activate extended mode
"use strict";

function CheckException(e) {
  var string = e.toString();
  assertTrue(string.indexOf("has already been declared") >= 0 ||
             string.indexOf("redeclaration") >= 0);  return 'Conflict';
}


function TestFunction(s,e) {
  try {
    return eval("(function(){" + s + ";return " + e + "})")();
  } catch (x) {
    return CheckException(x);
  }
}


function TestBlock(s,e) {
  try {
    return eval("(function(){ if (true) { " + s + "; }; return " + e + "})")();
  } catch (x) {
    return CheckException(x);
  }
}

function TestAll(expected,s,opt_e) {
  var e = "";
  var msg = s;
  if (opt_e) { e = opt_e; msg += "; " + opt_e; }
  assertEquals(expected, TestFunction(s,e), "function:'" + msg + "'");
  assertEquals(expected, TestBlock(s,e), "block:'" + msg + "'");
}


function TestConflict(s) {
  TestAll('Conflict', s);
  TestAll('Conflict', 'eval("' + s + '")');
}


function TestNoConflict(s) {
  TestAll('NoConflict', s, "'NoConflict'");
  TestAll('NoConflict', 'eval("' + s + '")', "'NoConflict'");
}

var letbinds = [ "let x",
                 "let x = 0",
                 "let x = undefined",
                 "function x() { }",
                 "let x = function() {}",
                 "let x, y",
                 "let y, x",
                 "const x = 0",
                 "const x = undefined",
                 "const x = function() {}",
                 "const x = 2, y = 3",
                 "const y = 4, x = 5",
                 ];
var varbinds = [ "var x",
                 "var x = 0",
                 "var x = undefined",
                 "var x = function() {}",
                 "var x, y",
                 "var y, x",
                 ];


for (var l = 0; l < letbinds.length; ++l) {
  // Test conflicting let/var bindings.
  for (var v = 0; v < varbinds.length; ++v) {
    // Same level.
    TestConflict(letbinds[l] +'; ' + varbinds[v]);
    TestConflict(varbinds[v] +'; ' + letbinds[l]);
    // Different level.
    TestConflict(letbinds[l] +'; {' + varbinds[v] + '; }');
    TestConflict('{ ' + varbinds[v] +'; }' + letbinds[l]);
  }

  // Test conflicting let/let bindings.
  for (var k = 0; k < letbinds.length; ++k) {
    // Same level.
    TestConflict(letbinds[l] +'; ' + letbinds[k]);
    TestConflict(letbinds[k] +'; ' + letbinds[l]);
    // Different level.
    TestNoConflict(letbinds[l] +'; { ' + letbinds[k] + '; }');
    TestNoConflict('{ ' + letbinds[k] +'; } ' + letbinds[l]);
  }

  // Test conflicting parameter/let bindings.
  TestConflict('(function (x) { ' + letbinds[l] + '; })()');
}

// Test conflicting catch/var bindings.
for (var v = 0; v < varbinds.length; ++v) {
  TestConflict('try {} catch (x) { ' + varbinds[v] + '; }');
}

// Test conflicting parameter/var bindings.
for (var v = 0; v < varbinds.length; ++v) {
  TestNoConflict('(function (x) { ' + varbinds[v] + '; })()');
}
