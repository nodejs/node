// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let fileName = 'mjsunit-assertion-error.js';

function addDefaultFrames(frameExpectations) {
  // The last frame contains the error instantiation.
  frameExpectations.unshift('assertTrue.*mjsunit.js');
  // Frist frame is the top-level script.
  frameExpectations.push(fileName);
}

function assertErrorMessage(frameExpectations, error) {
  let stack = error.stack.split("\n");
  let title = stack.shift();
  assertContains('MjsUnitAssertionError', title);
  addDefaultFrames(frameExpectations);
  // Add default frames to the expectations.
  assertEquals(frameExpectations.length, stack.length);
  for (let i = 0; i < stack.length; i++) {
    let frame = stack[i];
    let expectation = frameExpectations[i];
    assertTrue(frame.search(expectation) != -1,
        `Frame ${i}: Did not find '${expectation}' in '${frame}'`);
  }
}

// Toplevel
try {
  assertTrue(false);
} catch(e) {
  assertErrorMessage([], e);
}

// Single function.
function throwError() {
  assertTrue(false);
}
try {
  throwError();
  assertUnreachable();
} catch(e) {
  assertErrorMessage(['throwError'], e);
}

// Nested function.
function outer() {
  throwError();
}
try {
  outer();
  assertUnreachable();
} catch(e) {
  assertErrorMessage(['throwError', 'outer'], e);
}

// Test Array helper nesting
try {
  [1].map(throwError);
  assertUnreachable();
} catch(e) {
  assertErrorMessage(['throwError', 'Array.map'], e);
}
try {
  Array.prototype.map.call([1], throwError);
  assertUnreachable();
} catch(e) {
  assertErrorMessage(['throwError', 'Array.map'], e);
}

// Test eval
try {
  eval("assertTrue(false);");
  assertUnreachable();
} catch(e) {
  assertErrorMessage(['eval'], e);
}

(function testNestedEval() {
  try {
    eval("assertTrue(false);");
    assertUnreachable();
  } catch(e) {
    assertErrorMessage(['eval', 'testNestedEval'], e);
  }
})();


(function testConstructor() {
  class Failer {
    constructor() {
      assertTrue(false);
    }
  }
  try {
    new Failer();
    assertUnreachable();
  } catch(e) {
    assertErrorMessage(['new Failer', 'testConstructor'], e);
  }
})();
