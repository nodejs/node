// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --js-staging

async function TestThrowingSuppressedError() {
  class MyError extends Error {}
  const error1 = new MyError();
  const error2 = new MyError();
  const error3 = new MyError();

  try {
    await using x = {
      async[Symbol.asyncDispose]() {
        throw error1;
      }
    };
    await using y = {
      [Symbol.dispose]() {
        throw error2;
      }
    };
    throw error3;
  } catch (e) {
    assertTrue(
        e instanceof SuppressedError,
        'Expected an SuppressedError to have been thrown');
    assertEquals(
        e.error, error1,
        'Expected the outermost suppressing error to have been \'error1\'');
    assertTrue(
        e.suppressed instanceof SuppressedError,
        'Expected the outermost suppressed error to have been a SuppressedError');
    assertEquals(
        e.suppressed.error, error2,
        'Expected the innermost suppressing error to have been \'error2\'');
    assertEquals(
        e.suppressed.suppressed, error3,
        'Expected the innermost suppressed error to have been \'error3\'');
  }
}

async function RunTest() {
  await TestThrowingSuppressedError();
}

RunTest();
