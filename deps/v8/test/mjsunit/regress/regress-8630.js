// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Parameters can't have parentheses (both patterns and identifiers)
assertThrows("( ({x: 1}) ) => {};", SyntaxError);
assertThrows("( (x) ) => {}", SyntaxError);
assertThrows("( ({x: 1}) = y ) => {}", ReferenceError);
assertThrows("( (x) = y ) => {}", SyntaxError);

// Declarations can't have parentheses (both patterns and identifiers)
assertThrows("let [({x: 1})] = [];", SyntaxError);
assertThrows("let [(x)] = [];", SyntaxError);
assertThrows("let [({x: 1}) = y] = [];", SyntaxError);
assertThrows("let [(x) = y] = [];", SyntaxError);
assertThrows("var [({x: 1})] = [];", SyntaxError);
assertThrows("var [(x)] = [];", SyntaxError);
assertThrows("var [({x: 1}) = y] = [];", SyntaxError);
assertThrows("var [(x) = y] = [];", SyntaxError);

// Patterns can't have parentheses in assignments either
assertThrows("[({x: 1}) = y] = [];", ReferenceError);
assertThrows("({a,b}) = {a:2,b:3}", ReferenceError);

// Parentheses are fine around identifiers in assignments though, even inside a
// pattern
var x;
[(x)] = [2];
assertEquals(x, 2);
[(x) = 3] = [];
assertEquals(x, 3);
