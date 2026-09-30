// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests for break on caught exception in async modules.');

Protocol.Debugger.enable();
Protocol.Debugger.setPauseOnExceptions({state: 'caught'});
Protocol.Debugger.onPaused(({params: {data}}) => {
  InspectorTest.log('paused on exception:');
  InspectorTest.logMessage(data);
  Protocol.Debugger.resume();
});

contextGroup.addModule('await 1; try { throw "hello"; } catch (e) {}', 'module.js');

InspectorTest.waitForPendingTasks().then(() => InspectorTest.completeTest());
