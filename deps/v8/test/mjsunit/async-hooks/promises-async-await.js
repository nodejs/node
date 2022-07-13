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

// Check for executionAsyncId/triggerAsyncId when chained promises and
// async/await are combined
(function() {
  let p;
  let outerExecutionAsyncId = -1, outerTriggerAsyncId = -1;

  function inIrrelevantContext(resolve) {
    resolve(42);
  }

  function inContext1(foo) {
    foo();
  }

  function inContext2(foo) {
    foo();
  }

  outerExecutionAsyncId = async_hooks.executionAsyncId();
  outerTriggerAsyncId = async_hooks.triggerAsyncId();

  inContext1(() => {
    p = new Promise(resolve => {
      assertEquals(outerExecutionAsyncId, async_hooks.executionAsyncId());
      assertEquals(outerTriggerAsyncId, async_hooks.triggerAsyncId());
      inIrrelevantContext(resolve);
    }).then(() => {
      assertNotEquals(outerExecutionAsyncId, async_hooks.executionAsyncId());
      assertNotEquals(outerTriggerAsyncId, async_hooks.triggerAsyncId());
    }).catch((err) => {
      setTimeout(() => {
        throw err;
      }, 0);
    });
  });

  inContext2(async () => {
    assertEquals(outerExecutionAsyncId, async_hooks.executionAsyncId());
    assertEquals(outerTriggerAsyncId, async_hooks.triggerAsyncId());
    await p;
    assertNotEquals(outerExecutionAsyncId, async_hooks.executionAsyncId());
    assertNotEquals(outerTriggerAsyncId, async_hooks.triggerAsyncId());
  });

})();
