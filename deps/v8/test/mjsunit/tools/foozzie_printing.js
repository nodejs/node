// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Files: tools/clusterfuzz/js_fuzzer/resources/differential_fuzz_library.js

function test(expected, value) {
  assertEquals(expected, prettyPrinted(value));
}

// Primitives
test("undefined", undefined);
test("0", 0);
test("-0", -0);
test("42", 42);
test("42n", 42n);
test("2.3e-10", 2.3e-10);
test("Infinity", 1/0);
test("NaN", 0/0);
test('"foo"', "foo");

// Arrays
test("[]", []);
test("[1, 2, 3]", [1, 2, 3]);
test("[1, 2, , 3]", [1, 2, , 3]);
test('["a", "b", "c", "d"]', ["a", "b", "c", "d"]);

const arr = [];
arr[3] = 1;
arr[10] = 1
test("[, , , 1, , , , , , , 1]", arr);

// Nested arrays
test("[[], []]", [[],[]]);
test("[[[1, 2, 3]], [4, 5], 6]", [[[1, 2, 3]],[4, 5], 6]);

// Objects
test("Object{}", {});
test("Object{}", new Object());
test('Object{a: "b"}', {a: "b"});
test("Object{1: 2, 3: 4}", {1: 2, 3: 4});
test("A{}", new class A {}());

// Nested objects
test("Object{0: Object{}}", {0: {}});

// Nested arrays and objects
test("Object{0: [Object{}], 1: [Object{}]}", {0: [{}], 1: [{}]});

// Depth limit
test("[[[[...]]]]", [[[[[]]]]]);
test("Object{0: Object{0: Object{0: Object{0: ...}}}}",
   {0: {0: {0: {0: {}}}}});
test("Object{0: Object{0: Object{0: Object{0: ...}}, 1: [0, 1, [..., ...]]}}",
   {0: {0: {0: {0: {}}}, 1: [0, 1, [0, 1]]}});
