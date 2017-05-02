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

// Flags: --noalways-opt

Debug = debug.Debug

function TestCase(test_scenario, expected_output) {
  // Global variable, accessed from eval'd script.
  test_output = "";

  var script_text_generator = (function() {
    var variables = { a: 1, b: 1, c: 1, d: 1, e: 1, f: 1 };

    return {
      get: function() {
        return "(function() {\n " +
            "  function A() {\n " +
            "    test_output += 'a' + " + variables.a + ";\n " +
            "    test_output += '=';\n " +
            "    debugger;\n " +
            "    return 'Capybara';\n " +
            "  }\n " +
            "  function B(p1, p2) {\n " +
            "    test_output += 'b' + " + variables.b + ";\n " +
            "    return A();\n " +
            "  }\n " +
            "  function C() {\n " +
            "    test_output += 'c' + " + variables.c + ";\n " +
            "    // Function call with argument adaptor is intentional.\n " +
            "    return B();\n " +
            "  }\n " +
            "  function D() {\n " +
            "    test_output += 'd' + " + variables.d + ";\n " +
            "    // Function call with argument adaptor is intentional.\n " +
            "    return C(1, 2);\n " +
            "  }\n " +
            "  function E() {\n " +
            "    test_output += 'e' + " + variables.e + ";\n " +
            "    return D();\n " +
            "  }\n " +
            "  function F() {\n " +
            "    test_output += 'f' + " + variables.f + ";\n " +
            "    return E();\n " +
            "  }\n " +
            "  return F();\n " +
            "})\n";
      },
      change: function(var_name) {
        variables[var_name]++;
      }
    };
  })();

  var test_fun = eval(script_text_generator.get());

  var script = Debug.findScript(test_fun);

  var scenario_pos = 0;

  function DebuggerStatementHandler() {
    while (true) {
      assertTrue(scenario_pos < test_scenario.length);
      var change_var = test_scenario[scenario_pos++];
      if (change_var == '=') {
        // Continue.
        return;
      }
      script_text_generator.change(change_var);
      try {
        Debug.LiveEdit.SetScriptSource(script, script_text_generator.get(),
            false, []);
      } catch (e) {
        print("LiveEdit exception: " + e);
        throw e;
      }
    }
  }

  var saved_exception = null;

  function listener(event, exec_state, event_data, data) {
    if (event == Debug.DebugEvent.Break) {
      try {
        DebuggerStatementHandler();
      } catch (e) {
        saved_exception = e;
      }
    } else {
      print("Other: " + event);
    }
  }

  Debug.setListener(listener);
  assertEquals("Capybara", test_fun());
  Debug.setListener(null);

  if (saved_exception) {
    print("Exception: " + saved_exception);
    assertUnreachable();
  }

  print(test_output);

  assertEquals(expected_output, test_output);
}

TestCase(['='], "f1e1d1c1b1a1=");

TestCase(['c', '=', '='], "f1e1d1c1b1a1=c2b1a1=");

TestCase(['b', 'c', 'd', 'e', '=', '='], "f1e1d1c1b1a1=e2d2c2b2a1=");

TestCase(['b', 'c', '=', 'b', 'c', 'd', 'e', '=', '='], "f1e1d1c1b1a1=c2b2a1=e2d2c3b3a1=");

TestCase(['e', 'f', '=', '='], "f1e1d1c1b1a1=f2e2d1c1b1a1=");
