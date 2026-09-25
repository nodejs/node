// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
\u0063\u006f\u006e\u0073\u006f\u006c\u0065.\u006c\u006f\u0067("hello");

const some_loooooooong_user = "Alice";
const templateWithExpression = `User profile for ${some_loooooooong_user} is now active.`;
console.log(templateWithExpression);
