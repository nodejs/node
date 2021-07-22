// Copyright 2010 the V8 project authors. All rights reserved.
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

Debug = debug.Debug

unique_id = 1;

function TestBase(name) {
  print("TestBase constructor: " + name);

  const original_source = '/* ' + unique_id + '*/\n' +
      '(function ChooseAnimal(callback) {\n ' +
      '  callback();\n' +
      '  return \'Cat\';\n' +
      '})\n';
  const updated_source = original_source.replace('\'Cat\'', '\'Capybara\'');

  this.ChooseAnimal = eval(original_source);
  // Prevents eval script caching.
  unique_id++;

  const func = this.ChooseAnimal;

  var got_exception = false;
  var successfully_changed = false;

  // Should be called from Debug context.
  this.ScriptChanger = () => {
    assertEquals(false, successfully_changed, "applying patch second time");
    %LiveEditPatchScript(func, updated_source);
    successfully_changed = true;
  };
}

function Noop() {}

function WrapInCatcher(f, holder) {
  return function() {
    delete holder[0];
    try {
      f();
    } catch (e) {
      if (e.startsWith('LiveEdit failed')) {
        holder[0] = e;
      } else {
        throw e;
      }
    }
  };
}

function WrapInNativeCall(f) {
  return function() {
    return %Call(f, undefined);
  };
}

function ExecuteInDebugContext(f) {
  var result;
  var exception = null;
  Debug.setListener(function(event) {
    if (event == Debug.DebugEvent.Break) {
      try {
        result = f();
      } catch (e) {
        // Rethrow this exception later.
        exception = e;
      }
    }
  });
  debugger;
  Debug.setListener(null);
  if (exception !== null) throw exception;
  return result;
}

function WrapInDebuggerCall(f) {
  return function() {
    return ExecuteInDebugContext(f);
  };
}

function WrapInRestartProof(f) {
  var already_called = false;
  return function() {
    if (already_called) {
      return;
    }
    already_called = true;
    f();
  }
}

function WrapInConstructor(f) {
  return function() {
    return new function() {
      f();
    };
  }
}


// A series of tests. In each test we call ChooseAnimal function that calls
// a callback that attempts to modify the function on the fly.

test = new TestBase("First test ChooseAnimal without edit");
assertEquals("Cat", test.ChooseAnimal(Noop));

test = new TestBase("Test without function on stack");
test.ScriptChanger();
assertEquals("Capybara", test.ChooseAnimal(Noop));

test = new TestBase("Test with C++ frame above ChooseAnimal frame");
exception_holder = {};
assertEquals("Cat", test.ChooseAnimal(WrapInNativeCall(WrapInDebuggerCall(WrapInCatcher(test.ScriptChanger, exception_holder)))));
assertTrue(!!exception_holder[0]);
