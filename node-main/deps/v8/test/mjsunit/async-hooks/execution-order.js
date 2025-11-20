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

// Flags: --expose-async-hooks

// Check for correct execution of available hooks and asyncIds
(function() {
  let calledHooks = [];
  let rootAsyncId = 0;

  let ah = async_hooks.createHook({
    init: function init(asyncId, type, triggerAsyncId, resource) {
      if (type !== 'PROMISE') {
        return;
      }
      if (triggerAsyncId === 0) {
        rootAsyncId = asyncId;
      }
      calledHooks.push(['init', asyncId]);
    },
    promiseResolve: function promiseResolve(asyncId) {
      calledHooks.push(['resolve', asyncId]);
    },
    before: function before(asyncId) {
      calledHooks.push(['before', asyncId]);
    },
    after: function after(asyncId) {
      calledHooks.push(['after', asyncId]);
    },
  });
  ah.enable();

  new Promise(function(resolve) {
    resolve(42);
  }).then(function() {
    // [hook type, async Id]
    const expectedHooks = [
      ['init', rootAsyncId],  // the promise that we create initially
      ['resolve', rootAsyncId],
      ['init', rootAsyncId + 1],  // the chained promise with the assertions
      ['init', rootAsyncId + 2],  // the chained promise from the catch block
      ['before', rootAsyncId + 1],
      // ['after', rootAsyncId + 1] will get called after the assertions
    ];

    assertArrayEquals(expectedHooks, calledHooks,
      'Mismatch in async hooks execution order');
  }).catch((err) => {
    setTimeout(() => {
      throw err;
    }, 0);
  });
})();
