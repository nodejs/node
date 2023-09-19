// Copyright 2013 the V8 project authors. All rights reserved.
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

// Flags: --allow-natives-syntax --concurrent-recompilation --no-always-turbofan

if (!%IsConcurrentRecompilationSupported()) {
  print("Concurrent recompilation is disabled. Skipping this test.");
  quit();
}

function new_object() {
  var o = {};
  o.a = 1;
  o.b = 2;
  return o;
}

function add_field(obj) {
  // Assign twice to make the field non-constant.
  obj.c = 0;
  obj.c = 3;
}
%PrepareFunctionForOptimization(add_field);
var obj1 = new_object();
var obj2 = new_object();
add_field(obj1);
add_field(obj2);
%DisableOptimizationFinalization();
%OptimizeFunctionOnNextCall(add_field, "concurrent");

var o = new_object();
// Kick off recompilation.
add_field(o);
// Invalidate transition map after compile graph has been created.
%WaitForBackgroundOptimization();
o.c = 2.2;
assertUnoptimized(add_field);
// Sync with background thread to conclude optimization that bailed out.
%FinalizeOptimization();
if (!%IsDictPropertyConstTrackingEnabled()) {
  // TODO(v8:11457) Currently, we cannot inline property stores if there is a
  // dictionary mode prototype on the prototype chain. Therefore, if
  // v8_dict_property_const_tracking is enabled, the optimized code only
  // contains a call to the IC handler and doesn't get invalidated when the
  // transition map changes.
  assertUnoptimized(add_field);
}
// Clear type info for stress runs.
%ClearFunctionFeedback(add_field);
