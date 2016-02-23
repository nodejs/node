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

// Regression test of a very rare corner case where left-trimming an
// array caused invalid marking bit patterns on lazily swept pages.
//
// Lazy sweeping was deprecated. We are keeping the test case to make
// sure that concurrent sweeping, which relies on similar assumptions
// as lazy sweeping works correctly.

// Flags: --expose-gc --noincremental-marking --max-semi-space-size=1

(function() {
  var head = new Array(1);
  var tail = head;

  // Fill heap to increase old-space size and trigger concurrent sweeping on
  // some of the old-space pages.
  for (var i = 0; i < 200; i++) {
    tail[1] = new Array(1000);
    tail = tail[1];
  }
  array = new Array(100);
  gc(); gc();

  // At this point "array" should have been promoted to old-space and be
  // located in a concurrently swept page with intact marking bits. Now shift
  // the array to trigger left-trimming operations.
  assertEquals(100, array.length);
  for (var i = 0; i < 50; i++) {
    array.shift();
  }
  assertEquals(50, array.length);

  // At this point "array" should have been trimmed from the left with
  // marking bits being correctly transfered to the new object start.
  // Scavenging operations cause concurrent sweeping to advance and verify
  // that marking bit patterns are still sane.
  for (var i = 0; i < 200; i++) {
    tail[1] = new Array(1000);
    tail = tail[1];
  }
})();
