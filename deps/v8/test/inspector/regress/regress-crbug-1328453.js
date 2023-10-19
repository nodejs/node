// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {contextGroup, Protocol} = InspectorTest.start(
  'Don\'t crash when live editing an unused inner function [crbug.com/1328453]');

contextGroup.addScript(`
function outerFn() {
  function innerFn() {
    console.log("aa");  // We'll edit the "aa".
  }
}`);

const updatedScript = `
function outerFn() {
  function innerFn() {
    console.log("aabb");
  }
}`;

(async () => {
  Protocol.Debugger.enable();
  const { params: {scriptId} } = await Protocol.Debugger.onceScriptParsed();

  const response = await Protocol.Debugger.setScriptSource({ scriptId, scriptSource: updatedScript });
  InspectorTest.logMessage(response);

  InspectorTest.completeTest();
})();
