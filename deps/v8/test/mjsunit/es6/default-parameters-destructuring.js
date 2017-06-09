// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


(function TestSloppyEvalScoping() {
  var x = 1;

  function f41({[eval("var x = 2; 'a'")]: w}, z = x) { return z; }
  assertEquals(1, f41({}));
  assertEquals(1, f41({a: 0}));
  function f42({[eval("var x = 2; 'a'")]: w}, z = eval("x")) { return z; }
  assertEquals(1, f42({}));
  assertEquals(1, f42({a: 0}));
  function f43({a: w = eval("var x = 2")}, z = x) { return z; }
  assertEquals(1, f43({}));
  assertEquals(1, f43({a: 0}));
  function f44({a: w = eval("var x = 2")}, z = eval("x")) { return z; }
  assertEquals(1, f44({}));
  assertEquals(1, f44({a: 0}));

  function f5({a = eval("var x = 2"), b = x}) { return b; }
  assertEquals(2, f5({}));
  assertEquals(1, f5({a: 0}));
  function f6({a = eval("var x = 2"), b = eval("x")}) { return b; }
  assertEquals(2, f6({}));
  assertEquals(1, f6({a: 0}));
  function f71({[eval("var x = 2; 'a'")]: w, b = x}) { return b; }
  assertEquals(2, f71({}));
  assertEquals(2, f71({a: 0}));
  function f72({[eval("var x = 2; 'a'")]: w, b = eval("x")}) { return b; }
  assertEquals(2, f72({}));
  assertEquals(2, f72({a: 0}));
  function f73({a: w = eval("var x = 2"), b = x}) { return b; }
  assertEquals(2, f73({}));
  assertEquals(1, f73({a: 0}));
  function f74({a: w = eval("var x = 2"), b = eval("x")}) { return b; }
  assertEquals(2, f74({}));
  assertEquals(1, f74({a: 0}));

  var g41 = ({[eval("var x = 2; 'a'")]: w}, z = x) => { return z; };
  assertEquals(1, g41({}));
  assertEquals(1, g41({a: 0}));
  var g42 = ({[eval("var x = 2; 'a'")]: w}, z = eval("x")) => { return z; };
  assertEquals(1, g42({}));
  assertEquals(1, g42({a: 0}));
  var g43 = ({a: w = eval("var x = 2")}, z = x) => { return z; };
  assertEquals(1, g43({}));
  assertEquals(1, g43({a: 0}));
  var g44 = ({a: w = eval("var x = 2")}, z = eval("x")) => { return z; };
  assertEquals(1, g44({}));
  assertEquals(1, g44({a: 0}));

  var g5 = ({a = eval("var x = 2"), b = x}) => { return b; };
  assertEquals(2, g5({}));
  assertEquals(1, g5({a: 0}));
  var g6 = ({a = eval("var x = 2"), b = eval("x")}) => { return b; };
  assertEquals(2, g6({}));
  assertEquals(1, g6({a: 0}));
  var g71 = ({[eval("var x = 2; 'a'")]: w, b = x}) => { return b; };
  assertEquals(2, g71({}));
  assertEquals(2, g71({a: 0}));
  var g72 = ({[eval("var x = 2; 'a'")]: w, b = eval("x")}) => { return b; };
  assertEquals(2, g72({}));
  assertEquals(2, g72({a: 0}));
  var g73 = ({a: w = eval("var x = 2"), b = x}) => { return b; };
  assertEquals(2, g73({}));
  assertEquals(1, g73({a: 0}));
  var g74 = ({a: w = eval("var x = 2"), b = eval("x")}) => { return b; };
  assertEquals(2, g74({}));
  assertEquals(1, g74({a: 0}));
})();


(function TestStrictEvalScoping() {
  'use strict';
  var x = 1;

  function f41({[eval("var x = 2; 'a'")]: w}, z = x) { return z; }
  assertEquals(1, f41({}));
  assertEquals(1, f41({a: 0}));
  function f42({[eval("var x = 2; 'a'")]: w}, z = eval("x")) { return z; }
  assertEquals(1, f42({}));
  assertEquals(1, f42({a: 0}));
  function f43({a: w = eval("var x = 2")}, z = x) { return z; }
  assertEquals(1, f43({}));
  assertEquals(1, f43({a: 0}));
  function f44({a: w = eval("var x = 2")}, z = eval("x")) { return z; }
  assertEquals(1, f44({}));
  assertEquals(1, f44({a: 0}));

  function f5({a = eval("var x = 2"), b = x}) { return b; }
  assertEquals(1, f5({}));
  assertEquals(1, f5({a: 0}));
  function f6({a = eval("var x = 2"), b = eval("x")}) { return b; }
  assertEquals(1, f6({}));
  assertEquals(1, f6({a: 0}));
  function f71({[eval("var x = 2; 'a'")]: w, b = x}) { return b; }
  assertEquals(1, f71({}));
  assertEquals(1, f71({a: 0}));
  function f72({[eval("var x = 2; 'a'")]: w, b = eval("x")}) { return b; }
  assertEquals(1, f72({}));
  assertEquals(1, f72({a: 0}));
  function f73({a: w = eval("var x = 2"), b = x}) { return b; }
  assertEquals(1, f73({}));
  assertEquals(1, f73({a: 0}));
  function f74({a: w = eval("var x = 2"), b = eval("x")}) { return b; }
  assertEquals(1, f74({}));
  assertEquals(1, f74({a: 0}));
})();
