// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start("Check that inspector correctly passes caught/uncaught information.");

contextGroup.addScript(
`function throwCaught() { try { throw new Error(); } catch (_) {} }
 function throwUncaught() { throw new Error(); }
 function throwInPromiseCaught() {
   var reject;
   new Promise(function(res, rej) { reject = rej; }).catch(() => {});
   reject();
 }
 function throwInPromiseUncaught() {
   new Promise(function promiseUncaught() { throw new Error(); });
 }
 function throwInMapConstructor() { new Map('a'); }
 function throwInAsyncIterator() {
   let it = (async function*() {})();
   it.next.call({});
 }
 function schedule(f) { setTimeout(f, 0); }
`);

Protocol.Debugger.enable();

Protocol.Debugger.setPauseOnExceptions({ "state": "all" });
Protocol.Debugger.onPaused(message => {
  InspectorTest.log("paused in " + message.params.callFrames[0].functionName);
  InspectorTest.log("uncaught: " + message.params.data.uncaught);
  Protocol.Debugger.resume();
});

Protocol.Runtime.evaluate({ "expression": "schedule(throwCaught);" })
  .then(() => Protocol.Runtime.evaluate(
      { "expression": "schedule(throwUncaught);" }))
  .then(() => Protocol.Runtime.evaluate(
      { "expression": "schedule(throwInPromiseCaught);"}))
  .then(() => Protocol.Runtime.evaluate(
      { "expression": "schedule(throwInPromiseUncaught);"}))
  .then(() => Protocol.Runtime.evaluate(
      { "expression": "schedule(throwInMapConstructor);"}))
  .then(() => Protocol.Runtime.evaluate(
      { "expression": "schedule(throwInAsyncIterator);"}))
 .then(() => InspectorTest.completeTest());
