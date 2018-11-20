// Copyright 2008 the V8 project authors. All rights reserved.
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

// Flags: --expose-debug-as debug
// The functions used for testing backtraces. They are at the top to make the
// testing of source line/column easier.
function f(x, y) {
  a=1;
};

function g() {
  new f(1);
};


// Get the Debug object exposed from the debug context global object.
Debug = debug.Debug

listenerCallCount = 0;
listenerExceptions = [];


function listener(event, exec_state, event_data, data) {
  try {
  if (event == Debug.DebugEvent.Break)
  {
    listenerCallCount++;

    // Check that mirror cache is cleared when entering debugger.
    assertEquals(0, debug.next_handle_, "Mirror cache not cleared");
    assertEquals(0, debug.mirror_cache_.length, "Mirror cache not cleared");

    // Get the debug command processor in paused state.
    var dcp = exec_state.debugCommandProcessor(false);

    // Make a backtrace request to create some mirrors.
    var json;
    json = '{"seq":0,"type":"request","command":"backtrace"}'
    dcp.processDebugJSONRequest(json);

    // Make sure looking up loaded scripts does not clear the cache.
    Debug.scripts();

    // Some mirrors where cached.
    assertFalse(debug.next_handle_ == 0, "Mirror cache not used");
    assertFalse(debug.mirror_cache_.length == 0, "Mirror cache not used");
  }
  } catch (e) {
    print(e);
    listenerExceptions.push(e);
  };
};

// Add the debug event listener.
Debug.setListener(listener);

// Enter the debugger twice.
debugger;
debugger;

assertEquals([], listenerExceptions, "Exception in listener");
// Make sure that the debug event listener vas invoked.
assertEquals(2, listenerCallCount, "Listener not called");
