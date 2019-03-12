// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The actual regression test
assertThrows("(import(foo)) =>", undefined, "Invalid destructuring assignment target");

// Other related tests
assertThrows("import(foo) =>", undefined, "Malformed arrow function parameter list");
assertThrows("(a, import(foo)) =>", undefined, "Invalid destructuring assignment target");
assertThrows("(1, import(foo)) =>", undefined, "Invalid destructuring assignment target");
assertThrows("(super(foo)) =>", undefined, "'super' keyword unexpected here");
assertThrows("(bar(foo)) =>", undefined, "Invalid destructuring assignment target");

// No syntax errors
assertThrows("[import(foo).then] = [1];", undefined, "foo is not defined");
assertThrows("[[import(foo).then]] = [[1]];", undefined, "foo is not defined");
