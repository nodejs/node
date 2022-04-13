// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {Protocol, contextGroup} = InspectorTest.start(
  `Tests that Runtime throws exceptions after enabling domain on scripts with errors.`);

(async function test(){
  // Log all exceptions thrown
  Protocol.Runtime.onExceptionThrown(exception => {
    InspectorTest.logMessage(exception);
  });
  // Add scripts with syntax and reference errors
  contextGroup.addScript(
    `
    var x = ;
    //# sourceURL=syntaxError.js`);
  contextGroup.addScript(
    `
    var x = y;
    //# sourceURL=referenceError.js`);
  InspectorTest.log('Enabling Runtime Domain.');
  await Protocol.Runtime.enable();
  InspectorTest.completeTest();
})();
