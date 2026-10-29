// Copyright 2009 the V8 project authors. All rights reserved.
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

Debug = debug.Debug

var exceptionThrown = false;  // Exception in debug event listener.
var after_compile_count = 0;
var compile_error_count = 0;
var current_source = '';  // Current source being compiled.
var source_count = 0;  // Total number of scources compiled.
var mute_listener = false;

function compileSource(source) {
  current_source = source;
  eval(current_source);
  source_count++;
}

function safeEval(code) {
  try {
    mute_listener = true;
    return eval('(' + code + ')');
  } catch (e) {
    assertEquals(void 0, e);
    return undefined;
  } finally {
    mute_listener = false;
  }
}

function listener(event, exec_state, event_data, data) {
  if (mute_listener) return;
  try {
    if (event == Debug.DebugEvent.BeforeCompile ||
        event == Debug.DebugEvent.AfterCompile ||
        event == Debug.DebugEvent.CompileError) {
      // Count the events.
      if (event == Debug.DebugEvent.AfterCompile) {
        after_compile_count++;
      } else if (event == Debug.DebugEvent.CompileError) {
        compile_error_count++;
      }

      // If the compiled source contains 'eval' there will be additional compile
      // events for the source inside eval.
      if (current_source.indexOf('eval') == 0) {
        // For source with 'eval' there will be compile events with substrings
        // as well as with with the exact source.
        assertTrue(current_source.indexOf(event_data.script().source()) >= 0);
      } else {
        // For source without 'eval' there will be a compile events with the
        // exact source.
        assertEquals(current_source, event_data.script().source());
      }

      // Check that we pick script name from //# sourceURL, iff present
      if (event == Debug.DebugEvent.AfterCompile) {
        assertEquals(current_source.indexOf('sourceURL') >= 0 ?
            'myscript.js' : undefined,
                     event_data.script().name());
      }
    }
  } catch (e) {
    exceptionThrown = true;
  }
};


// Add the debug event listener.
Debug.setListener(listener);

// Compile different sources.
compileSource('a=1');
compileSource('(function(){})');
compileSource('eval("a=2")');
source_count++;  // Using eval causes additional compilation event.
compileSource('eval("eval(\'(function(){return a;})\')")');
source_count += 2;  // Using eval causes additional compilation event.
compileSource('JSON.parse(\'{"a":1,"b":2}\')');
// Using JSON.parse does not causes additional compilation events.
compileSource('x=1; //# sourceURL=myscript.js');

try {
  compileSource('}');
} catch(e) {
}

// Make sure that the debug event listener was invoked.
assertFalse(exceptionThrown, "exception in listener")

// Number of before and after + error events should be the same.
assertEquals(compile_error_count, 1);

// Check the actual number of events (no compilation through the API as all
// source compiled through eval).
assertEquals(source_count, after_compile_count);

Debug.setListener(null);
