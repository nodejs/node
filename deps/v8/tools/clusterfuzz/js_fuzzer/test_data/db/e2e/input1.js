// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let obj = {};
let v1 = 0;
let v2 = 0;
let v3 = 0;

// Contains statements that are extracted:

class A {
  foo (){
    Object.assign(obj, []);
  }
}

class B extends A {
  constructor(param) {
    // Call.
    super(param + 1);
  }

  foo() {
    // Call.
    super.valueOf();

    // Update.
    --super[v1];
  }
}

// Valid assignments.
v3.p = 2;
v3.p = {
  'something': 12345,
  'large': 6789,
};

// Call.
Array.from(obj);

// Arrow-function.
(o=v1) => o + 1;

// Class (though this throws).
(class x extends x {});

// Logical.
v1 && v2();

// New.
new Set([...obj]);

// Sequence.
v1, v2;

// Tag template.
v3``;

// Just this.
this;

// Unary.
delete NaN;

// Update.
obj[5]++;

// Statements that are not extracted:
123;
-123;
v1;
v1 + v2;
[1, 2, 3];
obj[5];
obj[5] = 42;
obj["5"] = 42;
v3 = 1;
Array.from(v1, v2, v3);

obj = function (a){ return a; };
obj(5);

if (true) { 123; }
while (true) { 123; }

// TODO(machenbach): This should be valid in theory. Babel truncates the
// inner parentheses, the it becomes invalid. If we make this valid, however,
// we might wanna restrict the size and complexity as it might include a lot
// of test material.
(function (){ return 5; })();
