// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --strong-mode

"use strict";

function CheckSwitch() {
  let jumpStatements = [
    "break; ",
    "continue; ",
    "break foo; ",
    "continue foo; ",
    "return; ",
    "throw new TypeError(); ",
    "if(1) break; else continue; ",
    "if(1) {1+1; {break;}} else continue; "
  ]

  let otherStatements = [
    "null; ",
    "1+1; ",
    "{} ",
    "for(;false;) {break;} ",
    "for(;false;) {1+1; {throw new TypeError();}} ",
    "(function(){return});",
    "(function(){throw new TypeError();});",
    "{break; 1+1;} ",
    "if(1) break; ",
    "if(1) break; else 1+1; ",
    "if(1) 1+1; else break; ",
  ]

  let successContexts = [
    ["switch(1) {case 1: ", "case 2: }"],
    ["switch(1) {case 1: case 2: ", "default: }"],
    ["switch(1) {case 1: case 2: ", "default: {}}"],
    ["switch(1) {case 1: case 2: ", "default: 1+1}"],
    ["switch(1) {case 1: break; case 2: ", "default: }"],
    ["switch(1) {case 1: case 2: break; case 3: ", "case 4: default: }"],
    ["switch(1) {case 1: if(1) break; else {", "} default: break;}"]
  ]

  let strongThrowContexts = [
    ["switch(1) {case 1: 1+1; case 2: ", "}"],
    ["switch(1) {case 1: bar: break foo; case 2: ", "}"],
    ["switch(1) {case 1: bar:", " case 2: }"],
    ["switch(1) {case 1: bar:{ ", "} case 2: }"],
    ["switch(1) {case 1: bar:{ ", "} default: break;}"],
    ["switch(1) {case 1: { bar:{ { ", "} } } default: break;}"],
    ["switch(1) {case 1: { { { ", "} 1+1;} } default: break;}"],
    ["switch(1) {case 1: if(1) {", "} default: break;}"],
    ["switch(1) {case 1: bar:if(1) break; else {", "} default: break;}"]
  ]

  let sloppy_wrap = ["function f() { foo:for(;;) {", "}}"];
  let strong_wrap = ["function f() { 'use strong'; foo:for(;;) {", "}}"];

  for (let context of successContexts) {
    let sloppy_prefix = sloppy_wrap[0] + context[0];
    let sloppy_suffix = context[1] + sloppy_wrap[1];
    let strong_prefix = strong_wrap[0] + context[0];
    let strong_suffix = context[1] + strong_wrap[1];

    for (let code of jumpStatements) {
      assertDoesNotThrow(strong_wrap[0] + "switch(1) {case 1: " + code + "}}}");
      assertDoesNotThrow(strong_prefix + code + strong_suffix);
      assertDoesNotThrow(strong_prefix + "{ 1+1; " + code + "}" +
                         strong_suffix);
      assertDoesNotThrow(strong_prefix + "{ 1+1; { 1+1; " + code + "}}" +
                         strong_suffix);
      assertDoesNotThrow(strong_prefix + "if(1) " + code + "else break;" +
                         strong_suffix);
      assertDoesNotThrow(strong_prefix + "if(1) " + code +
                         "else if (1) break; else " + code + strong_suffix);
    }
    for (let code of otherStatements) {
        assertDoesNotThrow(sloppy_prefix + code + sloppy_suffix);
        assertThrows(strong_prefix + code + strong_suffix, SyntaxError);
    }
  }

  for (let context of strongThrowContexts) {
    let sloppy_prefix = sloppy_wrap[0] + context[0];
    let sloppy_suffix = context[1] + sloppy_wrap[1];
    let strong_prefix = strong_wrap[0] + context[0];
    let strong_suffix = context[1] + strong_wrap[1];

    for (let code of jumpStatements.concat(otherStatements)) {
      assertDoesNotThrow(sloppy_prefix + code + sloppy_suffix);
      assertThrows(strong_prefix + code + strong_suffix, SyntaxError);
    }
  }

  for (let code of otherStatements) {
    assertDoesNotThrow("switch(1) {default: " + code + "}");
    assertDoesNotThrow("switch(1) {case 1: " + code + "}");
    assertDoesNotThrow("switch(1) {case 1: default: " + code + "}");
    assertDoesNotThrow("switch(1) {case 1: break; default: " + code + "}");
    assertDoesNotThrow("switch(1) {case 1: " + code + "break; default: }");
  }
}

CheckSwitch();

assertDoesNotThrow("'use strong'; switch(1) {}");
assertDoesNotThrow("'use strong'; switch(1) {case 1:}");
assertDoesNotThrow("'use strong'; switch(1) {default:}");
assertDoesNotThrow("'use strong'; switch(1) {case 1: case 2: default:}");
