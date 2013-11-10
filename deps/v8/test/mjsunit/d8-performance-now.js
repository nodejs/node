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

// Flags: --allow-natives-syntax

// Test the performance.now() function of d8.  This test only makes sense with
// d8.

// Don't run this test in gc stress mode. Time differences may be long
// due to garbage collections.
%SetFlags("--gc-interval=-1");
%SetFlags("--nostress-compaction");

if (this.performance && performance.now) {
  (function run() {
    var start_test = performance.now();
    // Let the retry run for maximum 100ms to reduce flakiness.
    for (var start = performance.now();
        start - start_test < 100;
        start = performance.now()) {
      var end = performance.now();
      assertTrue(start >= start_test);
      assertTrue(end >= start);
      while (end - start == 0) {
        var next = performance.now();
        assertTrue(next >= end);
        end = next;
      }
      if (end - start <= 1) {
        // Found (sub-)millisecond granularity.
        return;
      } else {
        print("Timer difference too big: " + (end - start) + "ms");
      }
    }
    assertTrue(false);
  })()
}
