// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// This tests checks some possible wrong exception handling due to,
// for instance, the OSR loop peeling. If exception handlers are not updated
// correctly, when we run the second iteration of the outermost loop, which
// is the OSR optimised version, the try-catch will fail... which should not
// fail on a correct code.

function SingleLoop() {
  for (var a = 0; a < 2; a++) {
    try { throw 'The exception should have been caught.'; }
    catch(e) {}
    for (var b = 0; b < 1; b++) {
      %OptimizeOsr();
    }
  }
}


// These function could also fail if the exception handlers are not updated at
// the right time: a JSStackCheck gets created for the print, just after the
// bytecode for the while LoopHeader. If the OSR phase did not exit properly
// the exception before visiting the bytecode for the print, it will fail
// because some IfSuccess gets created for nothing (the IfException will
// become dead code and removed).
function EmptyBody() {
  try {; } catch(e) {; }
  var a = 0;
  while (1) {
    %OptimizeOsr();
    print("foo");

    if (a == 1) break;
    a++;
  }
}

function NestedLoops() {
  for (var a = 0; a < 2; a++) {
    try {; } catch(e) {; }
    %OptimizeOsr();
    var b = 0;
    while (1) {
      print("bar");

      if (b == 1) break;
      b++;
    }
  }
}


SingleLoop();
EmptyBody();
NestedLoops();
