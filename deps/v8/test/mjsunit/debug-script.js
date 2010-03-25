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

// Flags: --expose-debug-as debug --expose-gc
// Get the Debug object exposed from the debug context global object.
Debug = debug.Debug

Date();
RegExp();

// Count script types.
var named_native_count = 0;
var extension_count = 0;
var normal_count = 0;
var scripts = Debug.scripts();
for (i = 0; i < scripts.length; i++) {
  if (scripts[i].type == Debug.ScriptType.Native) {
    if (scripts[i].name) {
      named_native_count++;
    }
  } else if (scripts[i].type == Debug.ScriptType.Extension) {
    extension_count++;
  } else if (scripts[i].type == Debug.ScriptType.Normal) {
    normal_count++;
  } else {
    assertUnreachable('Unexpected type ' + scripts[i].type);
  }
}

// This has to be updated if the number of native scripts change.
assertEquals(14, named_native_count);
// If no snapshot is used, only the 'gc' extension is loaded.
// If snapshot is used, all extensions are cached in the snapshot.
assertTrue(extension_count == 1 || extension_count == 5);
// This script and mjsunit.js has been loaded.  If using d8, d8 loads
// a normal script during startup too.
assertTrue(normal_count == 2 || normal_count == 3);

// Test a builtins script.
var math_script = Debug.findScript('native math.js');
assertEquals('native math.js', math_script.name);
assertEquals(Debug.ScriptType.Native, math_script.type);

// Test a builtins delay loaded script.
var date_delay_script = Debug.findScript('native date.js');
assertEquals('native date.js', date_delay_script.name);
assertEquals(Debug.ScriptType.Native, date_delay_script.type);

// Test a debugger script.
var debug_delay_script = Debug.findScript('native debug.js');
assertEquals('native debug.js', debug_delay_script.name);
assertEquals(Debug.ScriptType.Native, debug_delay_script.type);

// Test an extension script.
var extension_gc_script = Debug.findScript('v8/gc');
if (extension_gc_script) {
  assertEquals('v8/gc', extension_gc_script.name);
  assertEquals(Debug.ScriptType.Extension, extension_gc_script.type);
}

// Test a normal script.
var mjsunit_js_script = Debug.findScript(/mjsunit.js/);
assertTrue(/mjsunit.js/.test(mjsunit_js_script.name));
assertEquals(Debug.ScriptType.Normal, mjsunit_js_script.type);

// Check a nonexistent script.
var dummy_script = Debug.findScript('dummy.js');
assertTrue(typeof dummy_script == 'undefined');
