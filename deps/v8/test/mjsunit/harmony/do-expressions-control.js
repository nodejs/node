// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --harmony-do-expressions

(function TestDoForInDoBreak() {
  function f(o, i) {
    var a = "result@" + do {
      var r = "(";
      for (var x in o) {
        var b = "end@" + do {
          if (x == i) { break } else { r += o[x]; x }
        }
      }
      r + ")";
    }
    return a + "," + b;
  }
  assertEquals("result@(3),end@0", f([3], 2));
  assertEquals("result@(35),end@1", f([3,5], 2));
  assertEquals("result@(35),end@1", f([3,5,7], 2));
  assertEquals("result@(35),end@1", f([3,5,7,9], 2));
  %OptimizeFunctionOnNextCall(f);
  assertEquals("result@(3),end@0", f([3], 2));
  assertEquals("result@(35),end@1", f([3,5], 2));
  assertEquals("result@(35),end@1", f([3,5,7], 2));
  assertEquals("result@(35),end@1", f([3,5,7,9], 2));
})();

(function TestDoForInDoContinue() {
  function f(o, i) {
    var a = "result@" + do {
      var r = "("
      for (var x in o) {
        var b = "end@" + do {
          if (x == i) { continue } else { r += o[x]; x }
        }
      }
      r + ")"
    }
    return a + "," + b
  }
  assertEquals("result@(3),end@0", f([3], 2));
  assertEquals("result@(35),end@1", f([3,5], 2));
  assertEquals("result@(35),end@1", f([3,5,7], 2));
  assertEquals("result@(359),end@3", f([3,5,7,9], 2));
  %OptimizeFunctionOnNextCall(f);
  assertEquals("result@(3),end@0", f([3], 2));
  assertEquals("result@(35),end@1", f([3,5], 2));
  assertEquals("result@(35),end@1", f([3,5,7], 2));
  assertEquals("result@(359),end@3", f([3,5,7,9], 2));
})();

(function TestDoForNestedWithTargetLabels() {
  function f(mode) {
    var loop = true;
    var head = "<";
    var tail = ">";
    var middle =
    "1" + do { loop1: for(; loop; head += "A") {
      "2" + do { loop2: for(; loop; head += "B") {
        "3" + do { loop3: for(; loop; head += "C") {
          "4" + do { loop4: for(; loop; head += "D") {
            "5" + do { loop5: for(; loop; head += "E") {
              "6" + do { loop6: for(; loop; head += "F") {
                loop = false;
                switch (mode) {
                  case "b1": break loop1;
                  case "b2": break loop2;
                  case "b3": break loop3;
                  case "b4": break loop4;
                  case "b5": break loop5;
                  case "b6": break loop6;
                  case "c1": continue loop1;
                  case "c2": continue loop2;
                  case "c3": continue loop3;
                  case "c4": continue loop4;
                  case "c5": continue loop5;
                  case "c6": continue loop6;
                  default: "7";
                }
              }}
            }}
          }}
        }}
      }}
    }}
    return head + middle + tail;
  }
  function test() {
    assertEquals(      "<1undefined>",      f("b1"));
    assertEquals(     "<A1undefined>",      f("c1"));
    assertEquals(     "<A12undefined>",     f("b2"));
    assertEquals(    "<BA12undefined>",     f("c2"));
    assertEquals(    "<BA123undefined>",    f("b3"));
    assertEquals(   "<CBA123undefined>",    f("c3"));
    assertEquals(   "<CBA1234undefined>",   f("b4"));
    assertEquals(  "<DCBA1234undefined>",   f("c4"));
    assertEquals(  "<DCBA12345undefined>",  f("b5"));
    assertEquals( "<EDCBA12345undefined>",  f("c5"));
    assertEquals( "<EDCBA123456undefined>", f("b6"));
    assertEquals("<FEDCBA123456undefined>", f("c6"));
    assertEquals("<FEDCBA1234567>",         f("xx"));
  }
  test();
  %OptimizeFunctionOnNextCall(f);
  test();
})();
