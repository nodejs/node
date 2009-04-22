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

// Test for const semantics.


function CheckException(e) {
  var string = e.toString();
  var index = string.indexOf(':');
  assertTrue(index >= 0);
  var name = string.slice(0, index);
  assertTrue(string.indexOf("has already been declared") >= 0 ||
             string.indexOf("redeclaration") >= 0);
  if (name == 'SyntaxError') return 'TypeError';
  return name;
}


function TestLocal(s,e) {
  try {
    return eval("(function(){" + s + ";return " + e + "})")();
  } catch (x) {
    return CheckException(x);
  }
}


// NOTE: TestGlobal usually only tests the given string in the context
// of a global object in dictionary mode. This is because we use
// delete to get rid of any added properties.
function TestGlobal(s,e) {
  // Collect the global properties before the call.
  var properties = [];
  for (var key in this) properties.push(key); 
  // Compute the result.
  var result;
  try {
    var code = s + (e ? "; $$$result=" + e : "");
    if (this.execScript) {
      execScript(code);
    } else {
      this.eval(code);
    }
    // Avoid issues if $$$result is not defined by
    // reading it through this.
    result = this.$$$result;
  } catch (x) {
    result = CheckException(x);
  }
  // Get rid of any introduced global properties before
  // returning the result.
  for (var key in this) {
    if (properties.indexOf(key) == -1) delete this[key];
  }
  return result;
}


function TestContext(s,e) {
  try {
    // Use a with-statement to force the system to do dynamic
    // declarations of the introduced variables or constants.
    with ({}) {
      return eval(s + ";" + e);
    }
  } catch (x) {
    return CheckException(x);
  }
}


function TestAll(expected,s,opt_e) {
  var e = "";
  var msg = s;
  if (opt_e) { e = opt_e; msg += "; " + opt_e; }
  assertEquals(expected, TestLocal(s,e), "local:'" + msg + "'");
  assertEquals(expected, TestGlobal(s,e), "global:'" + msg + "'");
  assertEquals(expected, TestContext(s,e), "context:'" + msg + "'");
}


function TestConflict(def0, def1) {
  // No eval.
  TestAll("TypeError", def0 +'; ' + def1);
  // Eval everything.
  TestAll("TypeError", 'eval("' + def0 + '; ' + def1 + '")');
  // Eval first definition.
  TestAll("TypeError", 'eval("' + def0 +'"); ' + def1);
  // Eval second definition.
  TestAll("TypeError", def0 + '; eval("' + def1 + '")');
  // Eval both definitions separately.
  TestAll("TypeError", 'eval("' + def0 +'"); eval("' + def1 + '")');  
}


// Test conflicting definitions.
TestConflict("const x", "var x");
TestConflict("const x = 0", "var x");
TestConflict("const x", "var x = 0");
TestConflict("const x = 0", "var x = 0");

TestConflict("var x", "const x");
TestConflict("var x = 0", "const x");
TestConflict("var x", "const x = 0");
TestConflict("var x = 0", "const x = 0");

TestConflict("const x = undefined", "var x");
TestConflict("const x", "var x = undefined");
TestConflict("const x = undefined", "var x = undefined");

TestConflict("var x = undefined", "const x");
TestConflict("var x", "const x = undefined");
TestConflict("var x = undefined", "const x = undefined");

TestConflict("const x = undefined", "var x = 0");
TestConflict("const x = 0", "var x = undefined");

TestConflict("var x = undefined", "const x = 0");
TestConflict("var x = 0", "const x = undefined");

TestConflict("const x", "function x() { }");
TestConflict("const x = 0", "function x() { }");
TestConflict("const x = undefined", "function x() { }");

TestConflict("function x() { }", "const x");
TestConflict("function x() { }", "const x = 0");
TestConflict("function x() { }", "const x = undefined");

TestConflict("const x, y", "var x");
TestConflict("const x, y", "var y");
TestConflict("const x = 0, y", "var x");
TestConflict("const x = 0, y", "var y");
TestConflict("const x, y = 0", "var x");
TestConflict("const x, y = 0", "var y");
TestConflict("const x = 0, y = 0", "var x");
TestConflict("const x = 0, y = 0", "var y");

TestConflict("var x", "const x, y");
TestConflict("var y", "const x, y");
TestConflict("var x", "const x = 0, y");
TestConflict("var y", "const x = 0, y");
TestConflict("var x", "const x, y = 0");
TestConflict("var y", "const x, y = 0");
TestConflict("var x", "const x = 0, y = 0");
TestConflict("var y", "const x = 0, y = 0");


// Test that multiple conflicts do not cause issues.
TestConflict("var x, y", "const x, y");


// Test that repeated const declarations throw redeclaration errors.
TestConflict("const x", "const x");
TestConflict("const x = 0", "const x");
TestConflict("const x", "const x = 0");
TestConflict("const x = 0", "const x = 0");

TestConflict("const x = undefined", "const x");
TestConflict("const x", "const x = undefined");
TestConflict("const x = undefined", "const x = undefined");

TestConflict("const x = undefined", "const x = 0");
TestConflict("const x = 0", "const x = undefined");

TestConflict("const x, y", "const x");
TestConflict("const x, y", "const y");
TestConflict("const x = 0, y", "const x");
TestConflict("const x = 0, y", "const y");
TestConflict("const x, y = 0", "const x");
TestConflict("const x, y = 0", "const y");
TestConflict("const x = 0, y = 0", "const x");
TestConflict("const x = 0, y = 0", "const y");

TestConflict("const x", "const x, y");
TestConflict("const y", "const x, y");
TestConflict("const x", "const x = 0, y");
TestConflict("const y", "const x = 0, y");
TestConflict("const x", "const x, y = 0");
TestConflict("const y", "const x, y = 0");
TestConflict("const x", "const x = 0, y = 0");
TestConflict("const y", "const x = 0, y = 0");


// Test that multiple const conflicts do not cause issues.
TestConflict("const x, y", "const x, y");


// Test that const inside loop behaves correctly.
var loop = "for (var i = 0; i < 3; i++) { const x = i; }";
TestAll(0, loop, "x");
TestAll(0, "var a,b,c,d,e,f,g,h; " + loop, "x");


// Test that const inside with behaves correctly.
TestAll(87, "with ({x:42}) { const x = 87; }", "x");
TestAll(undefined, "with ({x:42}) { const x; }", "x");
