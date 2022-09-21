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

// Flags: --allow-natives-syntax --concurrent-recompilation
// Flags: --no-always-turbofan

function f1(a, i) {
  return a[i] + 0.5;
}

%PrepareFunctionForOptimization(f1);
var arr = [0.0,,2.5];
assertEquals(0.5, f1(arr, 0));
assertEquals(0.5, f1(arr, 0));

// Optimized code of f1 depends on initial object and array maps.
%DisableOptimizationFinalization();
%OptimizeFunctionOnNextCall(f1, "concurrent");
// Kick off recompilation. Note that the NoElements protector is read by the
// compiler in the main-thread phase of compilation, i.e., before the store to
// Object.prototype below.
assertEquals(0.5, f1(arr, 0));
// Invalidate current initial object map after compile graph has been created.
%WaitForBackgroundOptimization();
Object.prototype[1] = 1.5;
assertEquals(2, f1(arr, 1));
assertUnoptimized(f1);
// Sync with background thread to conclude optimization, which bails out
// due to map dependency.
%FinalizeOptimization();
assertUnoptimized(f1, "sync");
// Clear type info for stress runs.
%ClearFunctionFeedback(f1);
