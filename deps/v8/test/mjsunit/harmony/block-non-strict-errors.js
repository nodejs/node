// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-scoping --harmony-classes

function CheckError(source) {
  var exception = null;
  try {
    eval(source);
  } catch (e) {
    exception = e;
  }
  assertNotNull(exception);
  assertEquals(
      "Block-scoped declarations (let, const, function, class) not yet supported outside strict mode",
      exception.message);
}


function CheckOk(source) {
  eval(source);
}

CheckError("let x = 1;");
CheckError("{ let x = 1; }");
CheckError("function f() { let x = 1; }");
CheckError("for (let x = 1; x < 1; x++) {}");
CheckError("for (let x of []) {}");
CheckError("for (let x in []) {}");
CheckError("class C {}");
CheckError("class C extends Array {}");
CheckError("(class {});");
CheckError("(class extends Array {});");
CheckError("(class C {});");
CheckError("(class C exends Array {});");

CheckOk("let = 1;");
CheckOk("{ let = 1; }");
CheckOk("function f() { let = 1; }");
CheckOk("for (let = 1; let < 1; let++) {}");
