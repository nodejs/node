// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Tests scripts hasing');

var hashes = new Set(["1C6D2E82E4E4F1BA4CB5762843D429DC872EBA18",
                      "EBF1ECD351E7A3294CB5762843D429DC872EBA18",
                      "86A31E7131896CF01BA837945C2894385F369F24"]);
Protocol.Debugger.enable();
Protocol.Debugger.onScriptParsed(function(messageObject)
{
  if (hashes.has(messageObject.params.hash))
    InspectorTest.log(`Hash received: ${messageObject.params.hash}`);
  else
    InspectorTest.log(`[FAIL]: unknown hash ${messageObject.params.hash}`);
});

function longScript() {
  var longScript = "var b = 1;";
  for (var i = 0; i < 2024; ++i)
    longScript += "++b;";
}

Protocol.Runtime.enable();
Protocol.Runtime.compileScript({ expression: "1", sourceURL: "foo1.js", persistScript: true });
Protocol.Runtime.compileScript({ expression: "239", sourceURL: "foo2.js", persistScript: true });
Protocol.Runtime.compileScript({ expression: "(" + longScript + ")()", sourceURL: "foo3.js", persistScript: true }).then(step2);

function step2()
{
  InspectorTest.completeTest();
}
