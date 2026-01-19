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

// See: http://code.google.com/p/v8/issues/detail?id=1365

// Check that builtin methods are passed undefined as the receiver
// when called as functions through variables.

// Flags: --allow-natives-syntax

// Global variable.
var valueOf = Object.prototype.valueOf;
var hasOwnProperty = Object.prototype.hasOwnProperty;

function callGlobalValueOf() { valueOf(); }
function callGlobalHasOwnProperty() { valueOf(); }

assertEquals(Object.prototype, Object.prototype.valueOf());
assertThrows(callGlobalValueOf);
assertThrows(callGlobalHasOwnProperty);

Object.prototype.valueOf();

assertEquals(Object.prototype, Object.prototype.valueOf());
assertThrows(callGlobalValueOf);
assertThrows(callGlobalHasOwnProperty);

function CheckExceptionCallLocal() {
  var valueOf = Object.prototype.valueOf;
  var hasOwnProperty = Object.prototype.hasOwnProperty;
  var exception = false;
  try { valueOf(); } catch(e) { exception = true; }
  assertTrue(exception);
  exception = false;
  try { hasOwnProperty(); } catch(e) { exception = true; }
  assertTrue(exception);
}
CheckExceptionCallLocal();

function CheckExceptionCallParameter(f) {
  var exception = false;
  try { f(); } catch(e) { exception = true; }
  assertTrue(exception);
}
CheckExceptionCallParameter(Object.prototype.valueOf);
CheckExceptionCallParameter(Object.prototype.hasOwnProperty);

function CheckPotentiallyShadowedByEval() {
  var exception = false;
  try {
    eval("hasOwnProperty('x')");
  } catch(e) {
    exception = true;
  }
  assertTrue(exception);
}
CheckPotentiallyShadowedByEval();
