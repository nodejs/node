// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

function test_string_starts_with(str, search) {
  return str.startsWith(search);
}



%PrepareFunctionForOptimization(test_string_starts_with);
assertTrue(test_string_starts_with("seokho", 'se'));
assertFalse(test_string_starts_with("seokho", 'dev'));

%OptimizeFunctionOnNextCall(test_string_starts_with);

assertTrue(test_string_starts_with("seokho", 'se'));
assertFalse(test_string_starts_with("seokho", 'dev'));

assertOptimized(test_string_starts_with);


function test_string_starts_with_pos(str, search, pos) {
  return str.startsWith(search, pos);
}

%PrepareFunctionForOptimization(test_string_starts_with_pos);
assertTrue(test_string_starts_with_pos("seokho", 'ho', 4));
assertFalse(test_string_starts_with_pos("seokho", 'seok', 4));

%OptimizeFunctionOnNextCall(test_string_starts_with_pos);
assertTrue(test_string_starts_with_pos("seokho", 'ho', 4));
assertFalse(test_string_starts_with_pos("seokho", 'seok', 4));

assertOptimized(test_string_starts_with_pos);
