// Copyright 2018 the V8 project authors. All rights reserved.
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

// Check for correct interleaving of Promises and async/await
(function () {
  const iterations = 10;
  let promiseCounter = iterations;
  let awaitCounter = 0;

  async function check(v) {
    awaitCounter = v;
    // The following checks ensure that "await" takes 3 ticks on the
    // microtask queue. Note: this will change in the future
    if (awaitCounter === 0) {
      assertEquals(iterations, promiseCounter);
    } else if (awaitCounter <= Math.floor(iterations / 3)) {
      assertEquals(iterations - awaitCounter * 3, promiseCounter);
    } else {
      assertEquals(0, promiseCounter);
    }
  }

  async function f() {
    for (let i = 0; i < iterations; i++) {
      await check(i);
    }
    return 0;
  }

  function countdown(v) {
    promiseCounter = v;
    if (v > 0) Promise.resolve(v - 1).then(countdown);
  }

  countdown(iterations);
  f();
})();
