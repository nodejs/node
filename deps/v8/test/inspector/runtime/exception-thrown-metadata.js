// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start("Check that exceptionThrown is supported by test runner.")

Protocol.Runtime.enable();
Protocol.Runtime.onExceptionThrown(message => InspectorTest.logMessage(message));
contextGroup.addScript("throw inspector.newExceptionWithMetaData('myerror', 'foo', 'bar');");
Protocol.Runtime.evaluate({ expression: "setTimeout(() => { \n  throw inspector.newExceptionWithMetaData('myerror2', 'foo2', 'bar2'); }, 0)" });
Protocol.Runtime.evaluate({ expression: "setTimeout(() => { \n  throw 2; }, 0)" });
Protocol.Runtime.evaluate({ expression: "setTimeout(() => { \n  throw {}; }, 0)" });
InspectorTest.waitForPendingTasks().then(InspectorTest.completeTest);
