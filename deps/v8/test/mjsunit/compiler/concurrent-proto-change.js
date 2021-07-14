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

// Flags: --allow-natives-syntax --no-always-opt
// Flags: --concurrent-recompilation --block-concurrent-recompilation
// Flags: --no-always-opt --no-turbo-concurrent-get-property-access-info

if (!%IsConcurrentRecompilationSupported()) {
  print("Concurrent recompilation is disabled. Skipping this test.");
  quit();
}

function f(foo) { return foo.bar(); }

%PrepareFunctionForOptimization(f);

var o = {};
o.__proto__ = { __proto__: { bar: function() { return 1; } } };

assertEquals(1, f(o));
assertEquals(1, f(o));

// Mark for concurrent optimization.
%OptimizeFunctionOnNextCall(f, "concurrent");
// Kick off recompilation.
assertEquals(1, f(o));
// Change the prototype chain after compile graph has been created.
o.__proto__.__proto__ = { bar: function() { return 2; } };
// At this point, concurrent recompilation thread has not yet done its job.
assertUnoptimized(f, "no sync");
// Let the background thread proceed.
%UnblockConcurrentRecompilation();
// Optimization eventually bails out due to map dependency.
assertUnoptimized(f, "sync");
assertEquals(2, f(o));
//Clear type info for stress runs.
%ClearFunctionFeedback(f);
