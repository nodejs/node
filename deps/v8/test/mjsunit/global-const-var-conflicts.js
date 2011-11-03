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

// Check that dynamically introducing conflicting consts/vars
// leads to exceptions.

var caught = 0;

eval("const a");
try { eval("var a"); } catch (e) { caught++; assertTrue(e instanceof TypeError); }
assertTrue(typeof a == 'undefined');
try { eval("var a = 1"); } catch (e) { caught++; assertTrue(e instanceof TypeError); }
assertTrue(typeof a == 'undefined');

eval("const b = 0");
try { eval("var b"); } catch (e) { caught++; assertTrue(e instanceof TypeError); }
assertEquals(0, b);
try { eval("var b = 1"); } catch (e) { caught++; assertTrue(e instanceof TypeError); }
assertEquals(0, b);

eval("var c");
try { eval("const c"); } catch (e) { caught++; assertTrue(e instanceof TypeError); }
assertTrue(typeof c == 'undefined');
try { eval("const c = 1"); } catch (e) { caught++; assertTrue(e instanceof TypeError); }
assertTrue(typeof c == 'undefined');

eval("var d = 0");
try { eval("const d"); } catch (e) { caught++; assertTrue(e instanceof TypeError); }
assertEquals(0, d);
try { eval("const d = 1"); } catch (e) { caught++; assertTrue(e instanceof TypeError); }
assertEquals(0, d);

assertEquals(8, caught);
