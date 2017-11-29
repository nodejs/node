// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests scripts hasing');

(async function test() {
  await Protocol.Debugger.enable();
  await Protocol.Runtime.enable();
  Protocol.Runtime.compileScript({
    expression: "1", sourceURL: "foo1.js", persistScript: true});
  let {params} = await Protocol.Debugger.onceScriptParsed();
  InspectorTest.logMessage(params);
  Protocol.Runtime.compileScript({
    expression: "239", sourceURL: "foo2.js", persistScript: true});
  ({params} = await Protocol.Debugger.onceScriptParsed());
  InspectorTest.logMessage(params);
  var script = "var b = 1;";
  for (var i = 0; i < 2024; ++i) {
    script += "++b;";
  }
  Protocol.Runtime.compileScript({
    expression: script, sourceURL: "foo3.js",
    persistScript: true});
  ({params} = await Protocol.Debugger.onceScriptParsed());
  InspectorTest.logMessage(params);
  InspectorTest.completeTest();
})()
