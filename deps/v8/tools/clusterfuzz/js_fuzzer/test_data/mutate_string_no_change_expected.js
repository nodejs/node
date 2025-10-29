// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original: mutate_string.js
console.log("hello");
function test(param) {
  var myVariable = 1;
  return arguments[0];
}
test("foo");
eval("var z = 1;");
__callSetAllocationTimeout(100, true);
var obj = {};
obj.property = 2;
var obj2 = {
  key: 3
};
const complexRegex = /user-(id[0-9]+)-group([A-Z]+)/;
"user-id12345-groupABC".match(complexRegex);
const parts = "valueAseparatorvalueBseparatorvalueC".split(/separator/);
console.log("hello");
const some_loooooooong_user = "Alice";
const templateWithExpression = `User profile for ${some_loooooooong_user} is now active.`;
console.log(templateWithExpression);
