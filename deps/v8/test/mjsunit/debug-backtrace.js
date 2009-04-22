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

listenerCalled = false;
exception = false;


function ParsedResponse(json) {
  this.response_ = eval('(' + json + ')');
  this.refs_ = [];
  if (this.response_.refs) {
    for (var i = 0; i < this.response_.refs.length; i++) {
      this.refs_[this.response_.refs[i].handle] = this.response_.refs[i];
    }
  }
}


ParsedResponse.prototype.response = function() {
  return this.response_;
}


ParsedResponse.prototype.body = function() {
  return this.response_.body;
}


ParsedResponse.prototype.lookup = function(handle) {
  return this.refs_[handle];
}


function listener(event, exec_state, event_data, data) {
  try {
  if (event == Debug.DebugEvent.Break)
  {
    // The expected backtrace is
    // 0: f
    // 1: g
    // 2: [anonymous]
    
    var response;
    var backtrace;
    var frame;
    var source;
    
    // Get the debug command processor.
    var dcp = exec_state.debugCommandProcessor();

    // Get the backtrace.
    var json;
    json = '{"seq":0,"type":"request","command":"backtrace"}'
    response = new ParsedResponse(dcp.processDebugJSONRequest(json));
    backtrace = response.body();
    assertEquals(0, backtrace.fromFrame);
    assertEquals(3, backtrace.toFrame);
    assertEquals(3, backtrace.totalFrames);
    var frames = backtrace.frames;
    assertEquals(3, frames.length);
    for (var i = 0; i < frames.length; i++) {
      assertEquals('frame', frames[i].type);
    }
    assertEquals(0, frames[0].index);
    assertEquals("f", response.lookup(frames[0].func.ref).name);
    assertEquals(1, frames[1].index);
    assertEquals("g", response.lookup(frames[1].func.ref).name);
    assertEquals(2, frames[2].index);
    assertEquals("", response.lookup(frames[2].func.ref).name);

    // Get backtrace with two frames.
    json = '{"seq":0,"type":"request","command":"backtrace","arguments":{"fromFrame":1,"toFrame":3}}'
    response = new ParsedResponse(dcp.processDebugJSONRequest(json));
    backtrace = response.body();
    assertEquals(1, backtrace.fromFrame);
    assertEquals(3, backtrace.toFrame);
    assertEquals(3, backtrace.totalFrames);
    var frames = backtrace.frames;
    assertEquals(2, frames.length);
    for (var i = 0; i < frames.length; i++) {
      assertEquals('frame', frames[i].type);
    }
    assertEquals(1, frames[0].index);
    assertEquals("g", response.lookup(frames[0].func.ref).name);
    assertEquals(2, frames[1].index);
    assertEquals("", response.lookup(frames[1].func.ref).name);

    // Get the individual frames.
    json = '{"seq":0,"type":"request","command":"frame"}'
    response = new ParsedResponse(dcp.processDebugJSONRequest(json));
    frame = response.body();
    assertEquals(0, frame.index);
    assertEquals("f", response.lookup(frame.func.ref).name);
    assertTrue(frame.constructCall);
    assertEquals(31, frame.line);
    assertEquals(3, frame.column);
    assertEquals(2, frame.arguments.length);
    assertEquals('x', frame.arguments[0].name);
    assertEquals('number', response.lookup(frame.arguments[0].value.ref).type);
    assertEquals(1, response.lookup(frame.arguments[0].value.ref).value);
    assertEquals('y', frame.arguments[1].name);
    assertEquals('undefined', response.lookup(frame.arguments[1].value.ref).type);

    json = '{"seq":0,"type":"request","command":"frame","arguments":{"number":0}}'
    response = new ParsedResponse(dcp.processDebugJSONRequest(json));
    frame = response.body();
    assertEquals(0, frame.index);
    assertEquals("f", response.lookup(frame.func.ref).name);
    assertEquals(31, frame.line);
    assertEquals(3, frame.column);
    assertEquals(2, frame.arguments.length);
    assertEquals('x', frame.arguments[0].name);
    assertEquals('number', response.lookup(frame.arguments[0].value.ref).type);
    assertEquals(1, response.lookup(frame.arguments[0].value.ref).value);
    assertEquals('y', frame.arguments[1].name);
    assertEquals('undefined', response.lookup(frame.arguments[1].value.ref).type);

    json = '{"seq":0,"type":"request","command":"frame","arguments":{"number":1}}'
    response = new ParsedResponse(dcp.processDebugJSONRequest(json));
    frame = response.body();
    assertEquals(1, frame.index);
    assertEquals("g", response.lookup(frame.func.ref).name);
    assertFalse(frame.constructCall);
    assertEquals(35, frame.line);
    assertEquals(2, frame.column);
    assertEquals(0, frame.arguments.length);

    json = '{"seq":0,"type":"request","command":"frame","arguments":{"number":2}}'
    response = new ParsedResponse(dcp.processDebugJSONRequest(json));
    frame = response.body();
    assertEquals(2, frame.index);
    assertEquals("", response.lookup(frame.func.ref).name);

    // Source slices for the individual frames (they all refer to this script).
    json = '{"seq":0,"type":"request","command":"source",' +
            '"arguments":{"frame":0,"fromLine":30,"toLine":32}}'
    response = new ParsedResponse(dcp.processDebugJSONRequest(json));
    source = response.body();
    assertEquals("function f(x, y) {", source.source.substring(0, 18));
    assertEquals(30, source.fromLine);
    assertEquals(32, source.toLine);
    
    json = '{"seq":0,"type":"request","command":"source",' +
            '"arguments":{"frame":1,"fromLine":31,"toLine":32}}'
    response = new ParsedResponse(dcp.processDebugJSONRequest(json));
    source = response.body();
    assertEquals("  a=1;", source.source.substring(0, 6));
    assertEquals(31, source.fromLine);
    assertEquals(32, source.toLine);
    
    json = '{"seq":0,"type":"request","command":"source",' +
            '"arguments":{"frame":2,"fromLine":35,"toLine":36}}'
    response = new ParsedResponse(dcp.processDebugJSONRequest(json));
    source = response.body();
    assertEquals("  new f(1);", source.source.substring(0, 11));
    assertEquals(35, source.fromLine);
    assertEquals(36, source.toLine);
    
    // Test line interval way beyond this script will result in an error.
    json = '{"seq":0,"type":"request","command":"source",' +
            '"arguments":{"frame":0,"fromLine":10000,"toLine":20000}}'
    response = new ParsedResponse(dcp.processDebugJSONRequest(json));
    assertFalse(response.response().success);
    
    // Test without arguments.
    json = '{"seq":0,"type":"request","command":"source"}'
    response = new ParsedResponse(dcp.processDebugJSONRequest(json));
    source = response.body();
    assertEquals(Debug.findScript(f).source, source.source);
    
    listenerCalled = true;
  }
  } catch (e) {
    exception = e
  };
};

// Add the debug event listener.
Debug.setListener(listener);

// Set a break point and call to invoke the debug event listener.
Debug.setBreakPoint(f, 0, 0);
g();

// Make sure that the debug event listener vas invoked.
assertFalse(exception, "exception in listener");
assertTrue(listenerCalled);

