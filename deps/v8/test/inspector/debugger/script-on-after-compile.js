// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start("Checks that inspector correctly process compiled scripts");

function addScripts() {
  // sourceURL in the same line
  return addScript("function foo1(){}//# sourceURL=oneline.js\n")
  // sourceURL without end line
    .then(() => addScript("function foo2(){}//# sourceURL=oneline-without-nl.js"))
  // other source urls
    .then(() => addScript("function foo3(){}\n//# sourceURL=twoline.js\n"))
    .then(() => addScript("function foo4(){}\n\n//# sourceURL=threeline.js\n"))

  // sourceMappingURL in the same line
    .then(() => addScript("function foo5(){}//# sourceMappingURL=oneline-map\n"))
  // sourceMappingURL without end line
    .then(() => addScript("function foo6(){}//# sourceMappingURL=oneline-without-nl-map"))
  // other sourceMappingURLs
    .then(() => addScript("function foo7(){}\n//# sourceMappingURL=twoline-map\n"))
    .then(() => addScript("function foo8(){}\n\n//# sourceMappingURL=threeline-map\n"))

  // sourceURL + sourceMappingURL
    .then(() => addScript("function foo9(){}//# sourceMappingURL=source-mapping-url-map\n//# sourceURL=source-url.js"))
    .then(() => addScript("function foo10(){}//# sourceURL=source-url.js\n//# sourceMappingURL=source-mapping-url-map"))

  // non zero endLine and endColumn..
    .then(() => addScript("function foo11(){}\n//# sourceURL=end1.js"))
  // .. + 1 character
    .then(() => addScript("function foo12(){}\n//# sourceURL=end2.js "))
  // script without sourceURL
    .then(() => addScript("function foo13(){}"))
  // script in eval
    .then(() => addScript("function foo15(){}; eval(\"function foo14(){}//# sourceURL=eval.js\")//# sourceURL=eval-wrapper.js"))
  // // inside sourceURL
    .then(() => addScript("{a:2:\n//# sourceURL=http://a.js"))
  // sourceURL and sourceMappingURL works even for script with syntax error
    .then(() => addScript("}//# sourceURL=failed.js\n//# sourceMappingURL=failed-map"))
  // empty lines at end
    .then(() => addScript("function foo16(){}\n"))
    .then(() => addScript("function foo17(){}\n\n"))
    .then(() => addScript("function foo18(){}\n\n\n"))
    .then(() => addScript("function foo19(){}\n\n\n\n"));
}

Protocol.Debugger.onScriptParsed((message) => requestSourceAndDump(message, true));
Protocol.Debugger.onScriptFailedToParse((message) => requestSourceAndDump(message, false));
addScripts()
  .then(() => Protocol.Debugger.enable())
  .then(addScripts)
  .then(() => Protocol.Debugger.disable())
  .then(() => InspectorTest.log("Remove script references and re-enable debugger."))
  .then(() => Protocol.Runtime.evaluate(
      { expression: "for (let i = 1; i < 20; ++i) eval(`foo${i} = undefined`);" }))
  .then(() => Protocol.HeapProfiler.collectGarbage())
  .then(() => Protocol.Debugger.enable())
  .then(InspectorTest.completeTest);

function addScript(source) {
  return Protocol.Runtime.evaluate({ expression: source });
}

function requestSourceAndDump(scriptParsedMessage, scriptParsed) {
  Protocol.Debugger.getScriptSource({ scriptId: scriptParsedMessage.params.scriptId })
    .then((sourceMessage) => dumpScriptParsed(scriptParsedMessage, sourceMessage, scriptParsed));
}

function dumpScriptParsed(scriptParsedMessage, sourceMessage, scriptParsed) {
  var sourceResult = sourceMessage.result;
  sourceResult.scriptSource = sourceResult.scriptSource.replace(/\n/g, "<nl>");
  InspectorTest.log(scriptParsed ? "scriptParsed" : "scriptFailedToParse");
  InspectorTest.logObject(sourceResult);
  InspectorTest.logMessage(scriptParsedMessage.params);
}
