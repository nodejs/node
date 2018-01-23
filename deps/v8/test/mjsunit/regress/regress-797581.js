// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --harmony-dynamic-import

function TryToLoadModule(filename, expect_error, token) {
  let caught_error;

  function SetError(e) {
    caught_error = e;
  }

  import(filename).catch(SetError);
  %RunMicrotasks();

  if (expect_error) {
    assertTrue(caught_error instanceof SyntaxError);
    assertEquals("Unexpected token " + token, caught_error.message);
  } else {
    assertEquals(undefined, caught_error);
  }
}

TryToLoadModule("modules-skip-regress-797581-1.js", true, ")");
TryToLoadModule("modules-skip-regress-797581-2.js", true, ")");
TryToLoadModule("modules-skip-regress-797581-3.js", true, "...");
TryToLoadModule("modules-skip-regress-797581-4.js", true, ",");
TryToLoadModule("modules-skip-regress-797581-5.js", false);
