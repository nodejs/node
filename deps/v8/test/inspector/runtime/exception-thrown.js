// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

print("Check that exceptionThrown is supported by test runner.")

Protocol.Runtime.enable();
Protocol.Runtime.onExceptionThrown(message => InspectorTest.logMessage(message));
Protocol.Runtime.evaluate({ expression: "setTimeout(() => { \n  throw new Error() }, 0)" });
Protocol.Runtime.evaluate({ expression: "setTimeout(\" }\", 0)" });
Protocol.Runtime.evaluate({ expression: "setTimeout(() => { \n  throw 239; }, 0)" });
InspectorTest.completeTestAfterPendingTimeouts();
