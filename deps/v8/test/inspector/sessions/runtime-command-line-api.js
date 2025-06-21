// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

InspectorTest.log('Tests that multiple sessions do not share command line api.');

(async function test() {
  var contextGroup = new InspectorTest.ContextGroup();
  var session1 = contextGroup.connect();
  await session1.Protocol.Runtime.enable();
  session1.Protocol.Runtime.onInspectRequested(message => {
    InspectorTest.log('inspectRequested from 1');
    InspectorTest.logMessage(message);
  });
  var session2 = contextGroup.connect();
  await session2.Protocol.Runtime.enable();
  session2.Protocol.Runtime.onInspectRequested(message => {
    InspectorTest.log('inspectRequested from 2');
    InspectorTest.logMessage(message);
  });

  InspectorTest.log('Setting $0 in 1');
  await session1.addInspectedObject(42);
  InspectorTest.log('Evaluating $0 in 1');
  InspectorTest.logMessage(await session1.Protocol.Runtime.evaluate({expression: '$0', includeCommandLineAPI: true}));
  InspectorTest.log('Evaluating $0 in 2');
  InspectorTest.logMessage(await session2.Protocol.Runtime.evaluate({expression: '$0', includeCommandLineAPI: true}));

  InspectorTest.log('Setting $0 in 2');
  await session2.addInspectedObject(17);
  InspectorTest.log('Evaluating $0 in 1');
  InspectorTest.logMessage(await session1.Protocol.Runtime.evaluate({expression: '$0', includeCommandLineAPI: true}));
  InspectorTest.log('Evaluating $0 in 2');
  InspectorTest.logMessage(await session2.Protocol.Runtime.evaluate({expression: '$0', includeCommandLineAPI: true}));

  InspectorTest.log('Setting $_ in 1');
  await session1.Protocol.Runtime.evaluate({expression: '42', objectGroup: 'console', includeCommandLineAPI: true});
  InspectorTest.log('Evaluating $_ in 1');
  InspectorTest.logMessage(await session1.Protocol.Runtime.evaluate({expression: '$_', includeCommandLineAPI: true}));
  InspectorTest.log('Evaluating $_ in 2');
  InspectorTest.logMessage(await session2.Protocol.Runtime.evaluate({expression: '$_', includeCommandLineAPI: true}));

  InspectorTest.log('Setting $_ in 2');
  await session2.Protocol.Runtime.evaluate({expression: '17', objectGroup: 'console', includeCommandLineAPI: true});
  InspectorTest.log('Evaluating $_ in 1');
  InspectorTest.logMessage(await session1.Protocol.Runtime.evaluate({expression: '$_', includeCommandLineAPI: true}));
  InspectorTest.log('Evaluating $_ in 2');
  InspectorTest.logMessage(await session2.Protocol.Runtime.evaluate({expression: '$_', includeCommandLineAPI: true}));

  InspectorTest.log('Inspecting in 1');
  await session1.Protocol.Runtime.evaluate({expression: 'var inspect1=inspect; inspect(42)', includeCommandLineAPI: true});
  InspectorTest.log('Inspecting in 1 through variable');
  await session1.Protocol.Runtime.evaluate({expression: 'inspect1(42)', includeCommandLineAPI: true});
  InspectorTest.log('Inspecting in 2');
  await session2.Protocol.Runtime.evaluate({expression: 'var inspect2=inspect; inspect(17)', includeCommandLineAPI: true});
  InspectorTest.log('Inspecting in 2 through variable');
  await session2.Protocol.Runtime.evaluate({expression: 'inspect2(17)', includeCommandLineAPI: true});
  InspectorTest.log('Inspecting in 2 through variable from 1');
  await session2.Protocol.Runtime.evaluate({expression: 'inspect1(42)', includeCommandLineAPI: true});

  InspectorTest.log('Disconnecting 1');
  session1.disconnect();

  InspectorTest.log('Evaluating $0 in 2');
  InspectorTest.logMessage(await session2.Protocol.Runtime.evaluate({expression: '$0', includeCommandLineAPI: true}));
  InspectorTest.log('Evaluating $_ in 2');
  InspectorTest.logMessage(await session2.Protocol.Runtime.evaluate({expression: '$_', includeCommandLineAPI: true}));
  InspectorTest.log('Inspecting in 2');
  await session2.Protocol.Runtime.evaluate({expression: 'inspect(17)', includeCommandLineAPI: true});
  InspectorTest.log('Inspecting in 2 through variable from 1');
  await session2.Protocol.Runtime.evaluate({expression: 'inspect1(42)', includeCommandLineAPI: true});
  InspectorTest.log('Inspecting in 2 through variable');
  await session2.Protocol.Runtime.evaluate({expression: 'inspect2(17)', includeCommandLineAPI: true});

  InspectorTest.completeTest();
})();
