// Copyright 2012 the V8 project authors. All rights reserved.
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

// Get the Debug object exposed from the debug context global object.
Debug = debug.Debug

var exception = null;
var state = 1;
var expected_source_line_text = null;
var expected_function_name = null;

// Simple debug event handler which first time will cause 'step in' action
// to get into g.call and than check that execution is stopped inside
// function 'g'.
function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break) {
      if (state == 1) {
        exec_state.prepareStep(Debug.StepAction.StepIn, 2);
        state = 2;
      } else if (state == 2) {
        assertEquals(expected_source_line_text,
                     event_data.sourceLineText());
        assertEquals(expected_function_name, event_data.func().name());
        state = 3;
      }
    }
  } catch(e) {
    exception = e;
  }
};

// Add the debug event listener.
Debug.setListener(listener);


var c = {
  name: 'name ',
  get getter1() {
    return this.name;  // getter 1
  },
  get getter2() {
    return {  // getter 2
     'a': c.name
    };
  },
  set setter1(n) {
    this.name = n;  // setter 1
  }
};

c.__defineGetter__('y', function getterY() {
  return this.name;  // getter y
});

c.__defineGetter__(3, function getter3() {
  return this.name;  // getter 3
});

c.__defineSetter__('y', function setterY(n) {
  this.name = n;  // setter y
});

c.__defineSetter__(3, function setter3(n) {
  this.name = n;  // setter 3
});

var d = {
  'c': c,
};

function testGetter1_1() {
  expected_function_name = 'getter1';
  expected_source_line_text = '    return this.name;  // getter 1';
  debugger;
  var x = c.getter1;
}

function testGetter1_2() {
  expected_function_name = 'getter1';
  expected_source_line_text = '    return this.name;  // getter 1';
  debugger;
  var x = c['getter1'];
}

function testGetter1_3() {
  expected_function_name = 'getter1';
  expected_source_line_text = '    return this.name;  // getter 1';
  for (var i = 1; i < 2; i++) {
    debugger;
    var x = c['getter' + i];
  }
}

function testGetter1_4() {
  expected_function_name = 'getter1';
  expected_source_line_text = '    return this.name;  // getter 1';
  debugger;
  var x = d.c.getter1;
}

function testGetter1_5() {
  expected_function_name = 'getter1';
  expected_source_line_text = '    return this.name;  // getter 1';
  for (var i = 2; i != 1; i--);
  debugger;
  var x = d.c['getter' + i];
}

function testGetter2_1() {
  expected_function_name = 'getter2';
  expected_source_line_text = '    return {  // getter 2';
  for (var i = 2; i != 1; i--);
  debugger;
  var t = d.c.getter2.name;
}


function testGetterY_1() {
  expected_function_name = 'getterY';
  expected_source_line_text = '  return this.name;  // getter y';
  debugger;
  var t = d.c.y;
}

function testIndexedGetter3_1() {
  expected_function_name = 'getter3';
  expected_source_line_text = '  return this.name;  // getter 3';
  debugger;
  var r = d.c[3];
}

function testSetterY_1() {
  expected_function_name = 'setterY';
  expected_source_line_text = '  this.name = n;  // setter y';
  debugger;
  d.c.y = 'www';
}

function testIndexedSetter3_1() {
  expected_function_name = 'setter3';
  expected_source_line_text = '  this.name = n;  // setter 3';
  var i = 3
  debugger;
  d.c[3] = 'www';
}

function testSetter1_1() {
  expected_function_name = 'setter1';
  expected_source_line_text = '    this.name = n;  // setter 1';
  debugger;
  d.c.setter1 = 'aa';
}

function testSetter1_2() {
  expected_function_name = 'setter1';
  expected_source_line_text = '    this.name = n;  // setter 1';
  debugger;
  d.c['setter1'] = 'bb';
}

function testSetter1_3() {
  expected_function_name = 'setter1';
  expected_source_line_text = '    this.name = n;  // setter 1';
  for (var i = 2; i != 1; i--);
  debugger;
  d.c['setter' + i] = i;
}

var e = {
  name: 'e'
};
e.__proto__ = c;

function testProtoGetter1_1() {
  expected_function_name = 'getter1';
  expected_source_line_text = '    return this.name;  // getter 1';
  debugger;
  var x = e.getter1;
}

function testProtoSetter1_1() {
  expected_function_name = 'setter1';
  expected_source_line_text = '    this.name = n;  // setter 1';
  debugger;
  e.setter1 = 'aa';
}

function testProtoIndexedGetter3_1() {
  expected_function_name = 'getter3';
  expected_source_line_text = '  return this.name;  // getter 3';
  debugger;
  var x = e[3];
}

function testProtoIndexedSetter3_1() {
  expected_function_name = 'setter3';
  expected_source_line_text = '  this.name = n;  // setter 3';
  debugger;
  e[3] = 'new val';
}

function testProtoSetter1_2() {
  expected_function_name = 'setter1';
  expected_source_line_text = '    this.name = n;  // setter 1';
  for (var i = 2; i != 1; i--);
  debugger;
  e['setter' + i] = 'aa';
}

for (var n in this) {
  if (n.substr(0, 4) != 'test') {
    continue;
  }
  state = 1;
  this[n]();
  assertNull(exception);
  assertEquals(3, state);
}

// Get rid of the debug event listener.
Debug.setListener(null);
