// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function testSimpleRoundtrip() {
  function simple(a, b) {
    let c = a + b;
    return c * 2;
  }

  // Ensure simple is compiled and has bytecode
  simple(1, 2);

  const bytecodeObj = %GetBytecode(simple);

  assertEquals("object", typeof bytecodeObj);
  assertTrue(bytecodeObj.bytecode instanceof ArrayBuffer);
  assertTrue(bytecodeObj.constant_pool instanceof Array);
  assertTrue(bytecodeObj.handler_table instanceof ArrayBuffer);
  assertEquals("number", typeof bytecodeObj.frame_size);
  assertEquals("number", typeof bytecodeObj.parameter_count);
  assertEquals("number", typeof bytecodeObj.max_arguments);
  assertEquals("number", typeof bytecodeObj.incoming_new_target_or_generator_register);

  // simple(a, b) has 2 parameters + receiver = 3
  assertEquals(3, bytecodeObj.parameter_count);

  %InstallBytecode(simple, bytecodeObj);

  assertEquals(6, simple(1, 2));
  assertEquals(10, simple(2, 3));
}

function testSimpleTransplant() {
  function ret42() { return 42; }
  function ret43() { return 43; }

  ret42();
  ret43();

  const bytecodeObjRet42 = %GetBytecode(ret42);
  const bytecodeObjRet43 = %GetBytecode(ret43);

  %InstallBytecode(ret43, bytecodeObjRet42);

  assertEquals(42, ret43());
}

function testExceptionHandlers() {
  function withTryCatch() {
    try {
      throw new Error("catch me");
    } catch(e) {
      return "caught";
    }
  }

  withTryCatch();

  const bc = %GetBytecode(withTryCatch);
  assertTrue(bc.handler_table.byteLength > 0);

  %InstallBytecode(withTryCatch, bc);
  assertEquals("caught", withTryCatch());
}

function testConstants() {
  function withConstants() {
    return "Hello World!";
  }

  withConstants();

  const bc = %GetBytecode(withConstants);
  assertTrue(bc.constant_pool.length > 0);
  assertTrue(bc.constant_pool.includes("Hello World!"));

  %InstallBytecode(testConstants, bc);
  assertEquals("Hello World!", testConstants());
}

testSimpleRoundtrip();
testSimpleTransplant();
testExceptionHandlers();
testConstants();
