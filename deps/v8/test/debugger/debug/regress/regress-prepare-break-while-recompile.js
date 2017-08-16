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

// Flags: --concurrent-recompilation --block-concurrent-recompilation
// Flags: --no-always-opt

if (!%IsConcurrentRecompilationSupported()) {
  print("Concurrent recompilation is disabled. Skipping this test.");
  quit();
}

Debug = debug.Debug

function foo() {
  var x = 1;
  return x;
}

function bar() {
  var x = 2;
  return x;
}

foo();
foo();
// Mark and kick off recompilation.
%OptimizeFunctionOnNextCall(foo, "concurrent");
foo();

// Set break points on an unrelated function. This clears both optimized
// and (shared) unoptimized code on foo, and sets both to lazy-compile builtin.
// Clear the break point immediately after to deactivate the debugger.
// Do all of this after compile graph has been created.
Debug.setListener(function(){});
Debug.setBreakPoint(bar, 0, 0);
Debug.clearAllBreakPoints();
Debug.setListener(null);

// At this point, concurrent recompilation is still blocked.
assertUnoptimized(foo, "no sync");
// Let concurrent recompilation proceed.
%UnblockConcurrentRecompilation();

// Install optimized code when concurrent optimization finishes.
// This needs to be able to deal with shared code being a builtin.
assertUnoptimized(foo, "sync");
