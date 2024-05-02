// Copyright 2021 the V8 project authors. All rights reserved.
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
// Kick off recompilation.
assertEquals(0.5, f1(arr, 0));
%WaitForBackgroundOptimization();
// Invalidate current initial object map.
Object.prototype[1] = 1.5;
assertEquals(2, f1(arr, 1));
// Not yet optimized since concurrent recompilation is blocked.
assertUnoptimized(f1);
// Sync with background thread to conclude optimization, which does bailout due
// to map dependency, because the compiler read the NoElements protector before
// the store to Object.prototype above.
%FinalizeOptimization();
assertUnoptimized(f1);
assertEquals(2, f1(arr, 1));
// Clear type info for stress runs.
%ClearFunctionFeedback(f1);
