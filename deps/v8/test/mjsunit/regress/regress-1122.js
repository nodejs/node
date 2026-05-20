// Copyright 2011 the V8 project authors. All rights reserved.
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

// Test that we can handle function calls with up to 32766 arguments, and
// that function calls with more arguments throw an exception.  Apply a
// similar limit to the number of function parameters.

// See http://code.google.com/p/v8/issues/detail?id=1122 and
// http://code.google.com/p/v8/issues/detail?id=1413.

function function_with_n_params_and_m_args(n, m) {
  test_prefix = 'prefix ';
  test_suffix = ' suffix';
  var source = 'test_prefix + (function f(';
  for (var arg = 0; arg < n ; arg++) {
    if (arg != 0) source += ',';
    source += 'arg' + arg;
  }
  source += ') { return arg' + (n - n % 2) / 2 + '; })(';
  for (var arg = 0; arg < m ; arg++) {
    if (arg != 0) source += ',';
    source += arg;
  }
  source += ') + test_suffix';
  return eval(source);
}

assertEquals('prefix 4000 suffix',
             function_with_n_params_and_m_args(8000, 8000));
assertEquals('prefix 3000 suffix',
             function_with_n_params_and_m_args(6000, 8000));
assertEquals('prefix 5000 suffix',
             function_with_n_params_and_m_args(10000, 8000));
assertEquals('prefix 9000 suffix',
             function_with_n_params_and_m_args(18000, 18000));
assertEquals('prefix 16000 suffix',
             function_with_n_params_and_m_args(32000, 32000));
assertEquals('prefix undefined suffix',
             function_with_n_params_and_m_args(32000, 10000));

assertThrows("function_with_n_params_and_m_args(66000, 30000)");
assertThrows("function_with_n_params_and_m_args(30000, 66000)");
