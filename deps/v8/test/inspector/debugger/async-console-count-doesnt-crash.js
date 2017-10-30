// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start("setTimeout(console.count, 0) doesn't crash with enabled async stacks.")

Protocol.Debugger.enable();
Protocol.Debugger.setAsyncCallStackDepth({ maxDepth: 1 });
Protocol.Runtime.evaluate({ expression: "setTimeout(console.count, 0)" });
InspectorTest.waitForPendingTasks().then(InspectorTest.completeTest);
