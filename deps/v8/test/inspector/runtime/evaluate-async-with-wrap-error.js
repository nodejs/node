// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start("Test that " +
    "Runtime.evaluate correctly process errors during wrapping async result.");

var evaluateArguments = {
  expression: "Promise.resolve(Symbol(123))",
  returnByValue: true,
  awaitPromise: true
};
Protocol.Runtime.evaluate(evaluateArguments)
  .then(message => InspectorTest.logMessage(message))
  .then(() => InspectorTest.completeTest());
