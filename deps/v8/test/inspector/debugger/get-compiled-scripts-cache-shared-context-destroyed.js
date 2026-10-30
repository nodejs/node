// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --verify-heap

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that late Debugger.enable reports cache-shared scripts after a sharing context is destroyed');

(async function test() {
  const mainContextPromise = Protocol.Runtime.onceExecutionContextCreated();
  await Protocol.Runtime.enable();
  const {params: {context: {id: mainId}}} = await mainContextPromise;

  contextGroup.createContext('child1');
  const {params: {context: {id: child1Id}}} =
      await Protocol.Runtime.onceExecutionContextCreated();
  contextGroup.createContext('child2');
  const {params: {context: {id: child2Id}}} =
      await Protocol.Runtime.onceExecutionContextCreated();
  contextGroup.createContext('child3');
  const {params: {context: {id: child3Id}}} =
      await Protocol.Runtime.onceExecutionContextCreated();
  contextGroup.createContext('child4');
  const {params: {context: {id: child4Id}}} =
      await Protocol.Runtime.onceExecutionContextCreated();
  contextGroup.createContext('child5');
  const {params: {context: {id: child5Id}}} =
      await Protocol.Runtime.onceExecutionContextCreated();

  // Case 1: Main context compiles shared1.js first, child1 compiles it second
  // (hitting the compilation cache), and child1 is then destroyed.
  const shared1Source =
      'function shared1() { return 1; }\n//# sourceURL=shared1.js';
  await Protocol.Runtime.evaluate(
      {expression: shared1Source, contextId: mainId});
  await Protocol.Runtime.evaluate(
      {expression: shared1Source, contextId: child1Id});
  await Protocol.Runtime.evaluate({
    expression: 'inspector.fireContextDestroyed(); inspector.freeContext();',
    contextId: child1Id,
  });

  // Case 2: child2 compiles shared2.js first, main context compiles it second
  // (hitting the compilation cache while child2 is still alive), and child2 is
  // then destroyed.
  const shared2Source =
      'function shared2() { return 2; }\n//# sourceURL=shared2.js';
  await Protocol.Runtime.evaluate(
      {expression: shared2Source, contextId: child2Id});
  await Protocol.Runtime.evaluate(
      {expression: shared2Source, contextId: mainId});
  await Protocol.Runtime.evaluate({
    expression: 'inspector.fireContextDestroyed(); inspector.freeContext();',
    contextId: child2Id,
  });

  // Case 3: child3 compiles shared3.js first and is destroyed before main
  // context compiles shared3.js (hitting the compilation cache).
  const shared3Source =
      'function shared3() { return 3; }\n//# sourceURL=shared3.js';
  await Protocol.Runtime.evaluate(
      {expression: shared3Source, contextId: child3Id});
  await Protocol.Runtime.evaluate({
    expression: 'inspector.fireContextDestroyed(); inspector.freeContext();',
    contextId: child3Id,
  });
  await Protocol.Runtime.evaluate({expression: 'gc()', contextId: mainId});
  await Protocol.Runtime.evaluate(
      {expression: shared3Source, contextId: mainId});

  // Case 4: Three contexts (child4, main, child5) compile shared4.js while
  // alive, and then child4 and child5 are both destroyed.
  const shared4Source =
      'function shared4() { return 4; }\n//# sourceURL=shared4.js';
  await Protocol.Runtime.evaluate(
      {expression: shared4Source, contextId: child4Id});
  await Protocol.Runtime.evaluate(
      {expression: shared4Source, contextId: mainId});
  await Protocol.Runtime.evaluate(
      {expression: shared4Source, contextId: child5Id});
  await Protocol.Runtime.evaluate({
    expression: 'inspector.fireContextDestroyed(); inspector.freeContext();',
    contextId: child4Id,
  });

  // Case 5: child5 also compiles unshared.js and is destroyed; unshared.js
  // should not be reported when Debugger.enable is called later.
  const unsharedSource =
      'function unshared() { return 5; }\n//# sourceURL=unshared.js';
  await Protocol.Runtime.evaluate(
      {expression: unsharedSource, contextId: child5Id});
  await Protocol.Runtime.evaluate({
    expression: 'inspector.fireContextDestroyed(); inspector.freeContext();',
    contextId: child5Id,
  });

  // Case 6: Main context compiles shared5.js first, and a live context in a
  // separate context group compiles it second (hitting the compilation cache).
  const otherGroup = new InspectorTest.ContextGroup();
  const otherSession = otherGroup.connect();
  const otherContextPromise =
      otherSession.Protocol.Runtime.onceExecutionContextCreated();
  await otherSession.Protocol.Runtime.enable();
  const {params: {context: {id: otherMainId}}} = await otherContextPromise;
  const shared5Source =
      'function shared5() { return 6; }\n//# sourceURL=shared5.js';
  await Protocol.Runtime.evaluate(
      {expression: shared5Source, contextId: mainId});
  await otherSession.Protocol.Runtime.evaluate({
    expression: shared5Source,
    contextId: otherMainId,
  });

  await Protocol.Runtime.evaluate({expression: 'gc()', contextId: mainId});

  // Case 7: Main context compiles shared6.js first, child6 compiles it second,
  // and child6 fires contextDestroyed WITHOUT freeing the context or running
  // GC.
  contextGroup.createContext('child6');
  const {params: {context: {id: child6Id}}} =
      await Protocol.Runtime.onceExecutionContextCreated();
  const shared6Source =
      'function shared6() { return 7; }\n//# sourceURL=shared6.js';
  await Protocol.Runtime.evaluate(
      {expression: shared6Source, contextId: mainId});
  await Protocol.Runtime.evaluate(
      {expression: shared6Source, contextId: child6Id});
  await Protocol.Runtime.evaluate({
    expression: 'inspector.fireContextDestroyed();',
    contextId: child6Id,
  });

  const parsedUrls = [];
  Protocol.Debugger.onScriptParsed(function({params}) {
    if (params.url) {
      parsedUrls.push(`${params.url} (in main context: ${
          params.executionContextId === mainId})`);
    }
  });

  await Protocol.Debugger.enable();
  parsedUrls.sort();
  for (const entry of parsedUrls) {
    InspectorTest.log(entry);
  }

  const otherParsedUrls = [];
  otherSession.Protocol.Debugger.onScriptParsed(function({params}) {
    if (params.url) {
      otherParsedUrls.push(`${params.url} (in other context: ${
          params.executionContextId === otherMainId})`);
    }
  });
  await otherSession.Protocol.Debugger.enable();
  otherParsedUrls.sort();
  for (const entry of otherParsedUrls) {
    InspectorTest.log(entry);
  }

  // Case 8: Blackboxing shared2.js (originally compiled in destroyed child2)
  // must work when stepping in the main session.
  await Protocol.Debugger.setBlackboxPatterns({
    patterns: ['shared2\\.js'],
  });
  const evalPromise = Protocol.Runtime.evaluate({
    expression: 'function caller() { debugger; shared2(); }\ncaller();',
    contextId: mainId,
  });
  await Protocol.Debugger.oncePaused();
  await Protocol.Debugger.stepInto();
  await Protocol.Debugger.oncePaused();
  await Protocol.Debugger.stepInto();
  const {params: {callFrames}} = await Protocol.Debugger.oncePaused();
  InspectorTest.log(`Main session stepped to: ${callFrames[0].functionName}`);
  await Protocol.Debugger.resume();
  await evalPromise;

  InspectorTest.completeTest();
})();
