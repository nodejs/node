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

/**
 * This test uses assert{True,False}(... == ...) instead of
 * assertEquals(..., ...) to not rely on the details of the
 * implementation of assertEquals.
 */

assertTrue (void 0 == void 0, "void 0 == void 0");
assertTrue (null == null,     "null == null");
assertFalse(NaN == NaN,       "NaN == NaN");
assertFalse(NaN == 0,         "NaN == 0");
assertFalse(0 == NaN,         "0 == NaN");
assertFalse(NaN == Infinity,  "NaN == Inf");
assertFalse(Infinity == NaN,  "Inf == NaN");

assertTrue(Number.MAX_VALUE == Number.MAX_VALUE, "MAX == MAX");
assertTrue(Number.MIN_VALUE == Number.MIN_VALUE, "MIN == MIN");
assertTrue(Infinity == Infinity,                 "Inf == Inf");
assertTrue(-Infinity == -Infinity,               "-Inf == -Inf");

assertTrue(0 == 0,   "0 == 0");
assertTrue(0 == -0,  "0 == -0");
assertTrue(-0 == 0,  "-0 == 0");
assertTrue(-0 == -0, "-0 == -0");

assertFalse(0.9 == 1,             "0.9 == 1");
assertFalse(0.999999 == 1,        "0.999999 == 1");
assertFalse(0.9999999999 == 1,    "0.9999999999 == 1");
assertFalse(0.9999999999999 == 1, "0.9999999999999 == 1");

assertTrue('hello' == 'hello', "'hello' == 'hello'");

assertTrue (true == true,   "true == true");
assertTrue (false == false, "false == false");
assertFalse(true == false,  "true == false");
assertFalse(false == true,  "false == true");

assertFalse(new Wrapper(null) == new Wrapper(null),   "new Wrapper(null) == new Wrapper(null)");
assertFalse(new Boolean(true) == new Boolean(true),   "new Boolean(true) == new Boolean(true)");
assertFalse(new Boolean(false) == new Boolean(false), "new Boolean(false) == new Boolean(false)");

(function () {
  var x = new Wrapper(null);
  var y = x, z = x;
  assertTrue(y == x);
})();

(function () {
  var x = new Boolean(true);
  var y = x, z = x;
  assertTrue(y == x);
})();

(function () {
  var x = new Boolean(false);
  var y = x, z = x;
  assertTrue(y == x);
})();

assertTrue(null == void 0,             "null == void 0");
assertTrue(void 0 == null,             "void 0 == null");
assertFalse(new Wrapper(null) == null, "new Wrapper(null) == null");
assertFalse(null == new Wrapper(null), "null == new Wrapper(null)");

assertTrue(1 == '1',       "1 == '1");
assertTrue(255 == '0xff',  "255 == '0xff'");
assertTrue(0 == '\r',      "0 == '\\r'");
assertTrue(1e19 == '1e19', "1e19 == '1e19'");

assertTrue(new Boolean(true) == true,   "new Boolean(true) == true");
assertTrue(new Boolean(false) == false, "new Boolean(false) == false");
assertTrue(true == new Boolean(true),   "true == new Boolean(true)");
assertTrue(false == new Boolean(false), "false == new Boolean(false)");

assertTrue(Boolean(true) == true,   "Boolean(true) == true");
assertTrue(Boolean(false) == false, "Boolean(false) == false");
assertTrue(true == Boolean(true),   "true == Boolean(true)");
assertTrue(false == Boolean(false), "false == Boolean(false)");

assertTrue(new Wrapper(true) == true,   "new Wrapper(true) == true");
assertTrue(new Wrapper(false) == false, "new Wrapper(false) == false");
assertTrue(true == new Wrapper(true),   "true = new Wrapper(true)");
assertTrue(false == new Wrapper(false), "false = new Wrapper(false)");

function Wrapper(value) {
  this.value = value;
  this.valueOf = function () { return this.value; };
}
